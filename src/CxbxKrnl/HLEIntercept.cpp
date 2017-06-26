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
// *   Cxbx->Win32->CxbxKrnl->HLEIntercept.cpp
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
#define _CXBXKRNL_INTERNAL

#include <cmath>
#include <iomanip> // For std::setfill and std::setw
#include "CxbxKrnl.h"
#include "Emu.h"
#include "EmuFS.h"
#include "EmuXTL.h"
#include "EmuShared.h"
#include "HLEDataBase.h"
#include "HLEIntercept.h"
#include "xxhash32.h"
#include <Shlwapi.h>

static xbaddr EmuLocateFunction(OOVPA *Oovpa, xbaddr lower, xbaddr upper);
static void  EmuInstallPatches(OOVPATable *OovpaTable, uint32 OovpaTableSize, Xbe::Header *pXbeHeader);
static inline void EmuInstallPatch(xbaddr FunctionAddr, void *Patch);

#include <shlobj.h>
#include <unordered_map>
#include <sstream>

std::unordered_map<std::string, xbaddr> g_SymbolAddresses;
bool g_HLECacheUsed = false;

// D3D build version
uint32 g_BuildVersion;

bool bLLE_APU = false; // Set this to true for experimental APU (sound) LLE
bool bLLE_GPU = false; // Set this to true for experimental GPU (graphics) LLE
bool bLLE_JIT = false; // Set this to true for experimental JIT

bool bXRefFirstPass; // For search speed optimization, set in EmuHLEIntercept, read in EmuLocateFunction
uint32 UnResolvedXRefs; // Tracks XRef location, used (read/write) in EmuHLEIntercept and EmuLocateFunction

std::string GetDetectedSymbolName(xbaddr address, int *symbolOffset)
{
	std::string result = "";
	int closestMatch = MAXINT;

	for (auto it = g_SymbolAddresses.begin(); it != g_SymbolAddresses.end(); ++it) {
		xbaddr symbolAddr = (*it).second;
		if (symbolAddr == NULL)
			continue;

		if (symbolAddr <= address)
		{
			int distance = address - symbolAddr;
			if (closestMatch > distance)
			{
				closestMatch = distance;
				result = (*it).first;
			}
		}
	}

	if (closestMatch < MAXINT)
	{
		*symbolOffset = closestMatch;
		return result;
	}

	*symbolOffset = 0;
	return "unknown";
}

void *GetEmuPatchAddr(std::string aFunctionName)
{
	std::string patchName = "XTL::EmuPatch_" + aFunctionName;
	void* addr = GetProcAddress(GetModuleHandle(NULL), patchName.c_str());
	return addr;
}

bool VerifySymbolAddressAgainstXRef(char *SymbolName, xbaddr Address, int XRef)
{
	// Temporary verification - is XREF_D3DTSS_TEXCOORDINDEX derived correctly?
	// TODO : Remove this when XREF_D3DTSS_TEXCOORDINDEX derivation is deemed stable
	xbaddr XRefAddr = XRefDataBase[XRef];
	if (XRefAddr == Address)
		return true;

	if (XRefAddr == XREF_ADDR_DERIVE) {
		printf("HLE: XRef #%d derived 0x%.08X -> %s\n", XRef, Address, SymbolName);
		XRefDataBase[XRef] = Address;
		return true;
	}

	char Buffer[256];
	sprintf(Buffer, "Verification of %s failed : XREF was 0x%p while lookup gave 0x%p", SymbolName, XRefAddr, Address);
	// For XREF_D3DTSS_TEXCOORDINDEX, Kabuki Warriors hits this case
	CxbxPopupMessage(Buffer);
	return false;
}

void *FindSymbolAddress(char *SymbolName, bool FailWhenNotFound = true)
{
	if (g_SymbolAddresses.find(SymbolName) == g_SymbolAddresses.end()) {
		if (FailWhenNotFound) {
			CxbxKrnlCleanup("Symbol '%s' not registered!", SymbolName);
			return nullptr; // never reached
		}

		EmuWarning("Symbol '%s' not registered!", SymbolName);
		return nullptr;
	}

	return (void *)(g_SymbolAddresses[SymbolName]);
}

void SetGlobalSymbols()
{
	// Process fallbacks :
	if (g_SymbolAddresses.find("D3D__TextureState") == g_SymbolAddresses.end()) {
		if (g_SymbolAddresses.find("D3DDeferredTextureState") != g_SymbolAddresses.end())
			// Keep compatibility with HLE caches that contain "D3DDeferredTextureState" instead of "D3D__TextureState"
			g_SymbolAddresses["D3D__TextureState"] = g_SymbolAddresses["D3DDeferredTextureState"];
	}

	// Lookup and set all required global symbols
	/*ignore*/FindSymbolAddress("D3DDEVICE");
	XTL::Xbox_D3D__RenderState_Deferred = (DWORD*)FindSymbolAddress("D3DDeferredRenderState");
	XTL::Xbox_D3D_TextureState = (DWORD*)FindSymbolAddress("D3D__TextureState");
	XTL::Xbox_g_Stream = (XTL::X_Stream *)FindSymbolAddress("g_Stream", false); // Optional - aerox2 hits this case
#ifndef PATCH_TEXTURES
	XTL::Xbox_D3DDevice_m_Textures = (XTL::X_D3DBaseTexture **)((byte *)XTL::g_XboxD3DDevice + (uint)FindSymbolAddress("offsetof(D3DDevice,m_Textures)"));
#endif
}

void CheckHLEExports()
{
	for (uint32 d = 0; d < HLEDataBaseCount; d++) {
		const HLEData *FoundHLEData = &HLEDataBase[d];
		auto OovpaTable = FoundHLEData->OovpaTable;
		for (size_t a = 0; a < FoundHLEData->OovpaTableSize / sizeof(OOVPATable); a++) {
			bool IsXRef = (OovpaTable[a].Flags & Flag_XRef) > 0;
			bool DontPatch = (OovpaTable[a].Flags & Flag_DontPatch) > 0;
			void* addr = GetEmuPatchAddr(std::string(OovpaTable[a].szFuncName));
			if (DontPatch) {
				if (IsXRef) {
					DbgPrintf("DISABLED and XREF : %s %d %s\n", FoundHLEData->Library, FoundHLEData->BuildVersion, OovpaTable[a].szFuncName);
				}
			}

			if (addr != nullptr) {
				if (DontPatch) {
					DbgPrintf("Patch available, but DISABLED : %s %d %s\n", FoundHLEData->Library, FoundHLEData->BuildVersion, OovpaTable[a].szFuncName);
				}

				if (IsXRef) {
					DbgPrintf("Patch available, but XREF : %s %d %s\n", FoundHLEData->Library, FoundHLEData->BuildVersion, OovpaTable[a].szFuncName);
				}
			}
			else {
				if (DontPatch) {
					//  No patch available, and DISABLED is correct
				}
				else {
					if (IsXRef) {
						// No patch available, and XREF is correct
					}
					else {
						// This message pops up almost 200 times, all are patches that
						// are being removed because emulation is done one step lower (reading
						// XDK state variables).
						// Disable this message, as it won't matter once we start refactoring
						// the HLE databases (removing the DISABLED/XREF/PATCH distinction,
						// and merging registation of all versions of a symbol into one line).
						// DbgPrintf("No patch available, but not DISABLED nor XREF : %s %d %s\n", FoundHLEData->Library, FoundHLEData->BuildVersion, OovpaTable[a].szFuncName);
					}
				}
			}
		}
	}
}

void CheckDerivedDevice(xbaddr pFunc, int iCodeOffsetFor_X_pDevice)
{
	// Read address of "g_pDevice" from D3DDevice_SetViewport
	xbaddr DerivedAddr_D3DDevice = *((xbaddr*)(pFunc + iCodeOffsetFor_X_pDevice));

	// Temporary verification - is XREF_D3DDEVICE derived correctly?
	// TODO : Remove this when D3DEVICE derivation is deemed stable
	if (VerifySymbolAddressAgainstXRef("D3DDEVICE", DerivedAddr_D3DDevice, XREF_D3DDEVICE)) {
		g_SymbolAddresses["D3DDEVICE"] = DerivedAddr_D3DDevice;
	}
}

void CheckDerivedRenderState(xbaddr pFunc, int iCodeOffsetFor_X_D3DRS, int XREF_D3DRS_, int X_D3DRS)
{
	using namespace XTL;

	// Read address of D3DRS_ from function
	xbaddr DerivedAddr_D3DRS_ = *((xbaddr*)(pFunc + iCodeOffsetFor_X_D3DRS));
	::DWORD XDK_D3DRS_ = DxbxMapMostRecentToActiveVersion[X_D3DRS];

	// Temporary verification - is XREF_D3DRS_ derived correctly?
	// TODO : Remove this when XREF_D3DRS_ derivation is deemed stable
	if (VerifySymbolAddressAgainstXRef("D3D__RenderState[D3DRS_]", DerivedAddr_D3DRS_, XREF_D3DRS_)) {

		::DWORD *Derived_D3D_RenderState = ((::DWORD*)DerivedAddr_D3DRS_) - XDK_D3DRS_;
		g_SymbolAddresses["D3D__RenderState"] = (xbaddr)Derived_D3D_RenderState;
		printf("HLE: Derived 0x%.08X -> D3D__RenderState\n", Derived_D3D_RenderState);

		// Derive address of Xbox_D3D__RenderState_Deferred from D3DRS_
		::DWORD *Derived_D3D__RenderState_Deferred = Derived_D3D_RenderState + DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST];
		g_SymbolAddresses["D3DDeferredRenderState"] = (xbaddr)Derived_D3D__RenderState_Deferred;
		printf("HLE: Derived 0x%.08X -> D3D__RenderState_Deferred\n", Derived_D3D__RenderState_Deferred);

		// Derive address of a few other deferred render state slots (to help xref-based function location)
		{
#define DeriveAndPrint(D3DRS) \
	XRefDataBase[XREF_##D3DRS] = (xbaddr)(Derived_D3D_RenderState + DxbxMapMostRecentToActiveVersion[X_##D3DRS]); \
	printf("HLE: Derived XREF_"#D3DRS"(%d) 0x%.08X -> D3D__RenderState[%d/*="#D3DRS"]\n", (int)XREF_##D3DRS, XRefDataBase[XREF_##D3DRS], DxbxMapMostRecentToActiveVersion[X_##D3DRS]);

			DeriveAndPrint(D3DRS_CULLMODE);
			DeriveAndPrint(D3DRS_FILLMODE);
			DeriveAndPrint(D3DRS_MULTISAMPLERENDERTARGETMODE);
			DeriveAndPrint(D3DRS_STENCILCULLENABLE);
			DeriveAndPrint(D3DRS_ROPZCMPALWAYSREAD);
			DeriveAndPrint(D3DRS_ROPZREAD);
			DeriveAndPrint(D3DRS_DONOTCULLUNCOMPRESSED);
#undef DeriveAndPrint
		}
	}
}

void CheckDerivedTextureState(xbaddr pFunc, int iCodeOffsetFor_X_D3DTSS_, int XREF_D3DTSS_, int X_D3DTSS_)
{
	using namespace XTL;
	
	// Read address of D3DTSS_TEXCOORDINDEX from D3DDevice_SetTextureState_TexCoordIndex
	xbaddr DerivedAddr_D3DTSS_ = *((xbaddr*)(pFunc + iCodeOffsetFor_X_D3DTSS_));
	if (VerifySymbolAddressAgainstXRef("D3D__TextureState[/*Stage*/0][D3DTSS_]", DerivedAddr_D3DTSS_, XREF_D3DTSS_)) {
		// Derive address of D3D_TextureState from D3DTSS_
		::DWORD *Derived_D3D_TextureState = ((::DWORD*)DerivedAddr_D3DTSS_) - DxbxFromNewVersion_D3DTSS(X_D3DTSS_);
		g_SymbolAddresses["D3D__TextureState"] = (xbaddr)Derived_D3D_TextureState;
		printf("HLE: Derived 0x%.08X -> D3D__TextureState\n", Derived_D3D_TextureState);
	}
}

void PrescanD3D(Xbe::Header *pXbeHeader)
{
	using namespace XTL;

	// Request a few fundamental XRefs to be derived instead of checked
	XRefDataBase[XREF_D3DDEVICE] = XREF_ADDR_DERIVE;
	XRefDataBase[XREF_D3DRS_CULLMODE] = XREF_ADDR_DERIVE;
	XRefDataBase[XREF_D3DRS_FILLMODE] = XREF_ADDR_DERIVE;
	XRefDataBase[XREF_D3DTSS_TEXCOORDINDEX] = XREF_ADDR_DERIVE;
	XRefDataBase[XREF_G_STREAM] = XREF_ADDR_DERIVE;
	XRefDataBase[XREF_OFFSET_D3DDEVICE_M_TEXTURES] = XREF_ADDR_DERIVE;

	DxbxBuildRenderStateMappingTable();

	xbaddr lower = pXbeHeader->dwBaseAddr;
	xbaddr upper = pXbeHeader->dwBaseAddr + pXbeHeader->dwSizeofImage;

	// Locate Xbox symbol "D3DDevice_SetViewport" and store it's address
	// and derive "_D3D__pDevice" (pre-4627 known as "g_pDevice") from it
	{
		extern LOOVPA<1 + 28> D3DDevice_SetViewport_3911;
		extern LOOVPA<1 + 10> D3DDevice_SetViewport_4034;
		extern LOOVPA<1 + 28> D3DDevice_SetViewport_4627;
		extern LOOVPA<1 + 9> D3DDevice_SetViewport_5028; // TODO : Recreate OOVPA
		extern LOOVPA<1 + 8> D3DDevice_SetViewport_5344; // TODO : Recreate OOVPA
		extern LOOVPA<1 + 8> D3DDevice_SetViewport_5558; // TODO : Recreate OOVPA
/*
D:\Patrick\git\Dxbx\Resources\Patterns\3911d3d8.pat(978 ):83EC085355568B35........8B860C0400008D8E502100003BC157750C8D8E68 FF 05CB 0160 _D3DDevice_SetViewport@4 ^0008D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0128R ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^0133R _D3DDevice_SetScissors@12 ^0139R _XMETAL_StartPush@4 ^0142R ?CommonSetViewport@D3D@@YIPAKPAVCDevice@1@PAK@Z 53148996140B0000E8........6A006A006A00E8........56E8........8BD08BCEE8........89068B46085F83C8018946085E5D5B83C408C204009090909090
D:\Patrick\git\Dxbx\Resources\Patterns\4361d3d8.pat(1052):83EC08568B35........8B86702000003B867C200000750C8B8E80200000894C FF 6C99 0160 _D3DDevice_SetViewport@4 ^0006D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0126R ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^0131R _D3DDevice_SetScissors@12 ^0140R ?MakeSpace@D3D@@YGPCKXZ ^0149R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ^0151D _D3D__DirtyFlags 8996E4090000E8........6A006A006A00E8........8B063B46045F5D5B7205E8........8BD08BCEE8........8906810D........000100005E83C408C20400
D:\Patrick\git\Dxbx\Resources\Patterns\4627d3d8.pat(1070):83EC08568B35........8B86B42100003B86C0210000750C8B8EC4210000894C FF FFBC 0160 _D3DDevice_SetViewport@4 ^0006D _D3D__pDevice ^0126R ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^0131R _D3DDevice_SetScissors@12 ^0140R ?MakeSpace@D3D@@YGPCKXZ ^0149R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ^0151D _D3D__DirtyFlags 8996A40A0000E8........6A006A006A00E8........8B063B46045F5D5B7205E8........8BD08BCEE8........8906810D........000100005E83C408C20400
D:\Patrick\git\Dxbx\Resources\Patterns\5344d3d8.pat(1079):83EC08538B5C241085DB578B3D........0F84270100008B87E41500003B87F4 FF 6CD7 0170 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^012ED _D3D__DirtyFlags ^0139D _D3D__DirtyFlags ^0145R _D3DDevice_SetScissors@12 ^014AR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^0156R _D3DDevice_MakeSpace@0 ^015FR ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z 8FD00A00008B53148997D40A0000A1........0D000100005EA3........5D6A006A006A00E8........E8........8B073B47047205E8........8BD08BCFE8........89075F5B83C408C20400909090
D:\Patrick\git\Dxbx\Resources\Patterns\5558d3d8.pat(1083):83EC0C558B6C241485ED568B35........0F84DC0100008B86F41900003B8604 EE 903D 0222 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^010ED __real@4f800000 ^0121D __real@3f000000 ^0129R ?FloatToLong@D3D@@YGJM@Z ^013FD __real@4f800000 ^014ER ?FloatToLong@D3D@@YGJM@Z ^0162D __real@4f800000 ^0175D __real@3f000000 ^017DR ?FloatToLong@D3D@@YGJM@Z ^0193D __real@4f800000 ^01A2R ?FloatToLong@D3D@@YGJM@Z ^01E3D _D3D__DirtyFlags ^01EED _D3D__DirtyFlags ^01FAR _D3DDevice_SetScissors@12 ^01FFR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^020BR _D3DDevice_MakeSpace@0 ^0214R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ........D88E580900008B9E5009000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65809000051D91C24E8........8BE885FF897C2410DB4424107D06D805........D88E5C0900008B9E5409000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65C09000051D91C24E8........8BF88B4424148B4C24188986D00E00002BE88B4424202BF989BEDC0E0000898ED40E000089AED80E00008B4810898EE00E00008B50148996E40E0000A1........0D000100005FA3........5B6A006A006A00E8........E8........8B063B46047205E8........8BD08BCEE8........89065E5D83C40CC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5659d3d8.pat(1083):83EC0C558B6C241485ED568B35........0F84DC0100008B86F41900003B8604 EE 903D 0222 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^010ED __real@4f800000 ^0121D __real@3f000000 ^0129R ?FloatToLong@D3D@@YGJM@Z ^013FD __real@4f800000 ^014ER ?FloatToLong@D3D@@YGJM@Z ^0162D __real@4f800000 ^0175D __real@3f000000 ^017DR ?FloatToLong@D3D@@YGJM@Z ^0193D __real@4f800000 ^01A2R ?FloatToLong@D3D@@YGJM@Z ^01E3D _D3D__DirtyFlags ^01EED _D3D__DirtyFlags ^01FAR _D3DDevice_SetScissors@12 ^01FFR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^020BR _D3DDevice_MakeSpace@0 ^0214R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ........D88E580900008B9E5009000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65809000051D91C24E8........8BE885FF897C2410DB4424107D06D805........D88E5C0900008B9E5409000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65C09000051D91C24E8........8BF88B4424148B4C24188986D00E00002BE88B4424202BF989BEDC0E0000898ED40E000089AED80E00008B4810898EE00E00008B50148996E40E0000A1........0D000100005FA3........5B6A006A006A00E8........E8........8B063B46047205E8........8BD08BCEE8........89065E5D83C40CC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5788d3d8.pat(1083):83EC0C558B6C241485ED568B35........0F84DC0100008B86041A00003B8614 EE 7800 0222 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^010ED __real@4f800000 ^0121D __real@3f000000 ^0129R ?FloatToLong@D3D@@YGJM@Z ^013FD __real@4f800000 ^014ER ?FloatToLong@D3D@@YGJM@Z ^0162D __real@4f800000 ^0175D __real@3f000000 ^017DR ?FloatToLong@D3D@@YGJM@Z ^0193D __real@4f800000 ^01A2R ?FloatToLong@D3D@@YGJM@Z ^01E3D _D3D__DirtyFlags ^01EED _D3D__DirtyFlags ^01FAR _D3DDevice_SetScissors@12 ^01FFR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^020BR _D3DDevice_MakeSpace@0 ^0214R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ........D88E5C0900008B9E5409000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65C09000051D91C24E8........8BE885FF897C2410DB4424107D06D805........D88E600900008B9E5809000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B66009000051D91C24E8........8BF88B4424148B4C24188986E00E00002BE88B4424202BF989BEEC0E0000898EE40E000089AEE80E00008B4810898EF00E00008B50148996F40E0000A1........0D000100005FA3........5B6A006A006A00E8........E8........8B063B46047205E8........8BD08BCEE8........89065E5D83C40CC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5849d3d8.pat(1083):83EC0C558B6C241485ED568B35........0F84DC0100008B86041A00003B8614 EE 7800 0222 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^010ED __real@4f800000 ^0121D __real@3f000000 ^0129R ?FloatToLong@D3D@@YGJM@Z ^013FD __real@4f800000 ^014ER ?FloatToLong@D3D@@YGJM@Z ^0162D __real@4f800000 ^0175D __real@3f000000 ^017DR ?FloatToLong@D3D@@YGJM@Z ^0193D __real@4f800000 ^01A2R ?FloatToLong@D3D@@YGJM@Z ^01E3D _D3D__DirtyFlags ^01EED _D3D__DirtyFlags ^01FAR _D3DDevice_SetScissors@12 ^01FFR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^020BR _D3DDevice_MakeSpace@0 ^0214R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ........D88E5C0900008B9E5409000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65C09000051D91C24E8........8BE885FF897C2410DB4424107D06D805........D88E600900008B9E5809000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B66009000051D91C24E8........8BF88B4424148B4C24188986E00E00002BE88B4424202BF989BEEC0E0000898EE40E000089AEE80E00008B4810898EF00E00008B50148996F40E0000A1........0D000100005FA3........5B6A006A006A00E8........E8........8B063B46047205E8........8BD08BCEE8........89065E5D83C40CC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5933d3d8.pat(1083):83EC0C558B6C241485ED568B35........0F84DC0100008B86041A00003B8614 EE 7800 0222 _D3DDevice_SetViewport@4 ^000DD _D3D__pDevice ^010ED __real@4f800000 ^0121D __real@3f000000 ^0129R ?FloatToLong@D3D@@YGJM@Z ^013FD __real@4f800000 ^014ER ?FloatToLong@D3D@@YGJM@Z ^0162D __real@4f800000 ^0175D __real@3f000000 ^017DR ?FloatToLong@D3D@@YGJM@Z ^0193D __real@4f800000 ^01A2R ?FloatToLong@D3D@@YGJM@Z ^01E3D _D3D__DirtyFlags ^01EED _D3D__DirtyFlags ^01FAR _D3DDevice_SetScissors@12 ^01FFR ?UpdateProjectionViewportTransform@D3D@@YGXXZ ^020BR _D3DDevice_MakeSpace@0 ^0214R ?CommonSetViewport@D3D@@YIPCKPAVCDevice@1@PCK@Z ........D88E5C0900008B9E5409000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B65C09000051D91C24E8........8BE885FF897C2410DB4424107D06D805........D88E600900008B9E5809000051D805........D91C24E8........3BC3762385DB895C2410DB4424107D06D805........D8B66009000051D91C24E8........8BF88B4424148B4C24188986E00E00002BE88B4424202BF989BEEC0E0000898EE40E000089AEE80E00008B4810898EF00E00008B50148996F40E0000A1........0D000100005FA3........5B6A006A006A00E8........E8........8B063B46047205E8........8BD08BCEE8........89065E5D83C40CC20400
*/

		xbaddr pFunc = NULL;
		int iCodeOffsetFor_X_pDevice = 0x0D; // verified for 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 5558)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_5558, lower, upper);
		else if (g_BuildVersion >= 5344)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_5344, lower, upper);
		else if (g_BuildVersion >= 5028)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_5028, lower, upper);
		else if (g_BuildVersion >= 4627) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_4627, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x06; // verified for 4627
		}
		else if (g_BuildVersion >= 4034) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_4034, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x06; // verified for 4361
		}
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetViewport_3911, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x08; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetViewport\n", pFunc);

			CheckDerivedDevice(pFunc, iCodeOffsetFor_X_pDevice);
		}
	}

	// Try to locate Xbox symbol "_D3D_RenderState" and store it's address (and a few derived)
	// via D3DDevice_SetRenderState_CullMode
	{
		extern LOOVPA<2 + 16> D3DDevice_SetRenderState_CullMode_3925;
		extern LOOVPA<2 + 14> D3DDevice_SetRenderState_CullMode_4034;
		extern LOOVPA<2 + 24> D3DDevice_SetRenderState_CullMode_4361;
		extern LOOVPA<2 + 13> D3DDevice_SetRenderState_CullMode_5233;
		#define D3DDevice_SetRenderState_CullMode_5344 D3DDevice_SetRenderState_CullMode_4361

/*
D:\Patrick\git\Dxbx\Resources\Patterns\3911d3d8.pat(115):568B35........56E8........8B4C240885C9C70008030400751289480483C0 05 B217 0070 _D3DDevice_SetRenderState_CullMode@4 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0009R _XMETAL_StartPush@4 ^0025D _D3D__RenderState+01FC ^0037D _D3D__RenderState+01F8 ^005AD _D3D__RenderState+01FC ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC204009090909090909090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\4361d3d8.pat(118):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0070 _D3DDevice_SetRenderState_CullMode@4 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^000FR ?MakeSpace@D3D@@YGPCKXZ ^002BD _D3D__RenderState+0200 ^003DD _D3D__RenderState+01FC ^0060D _D3D__RenderState+0200 ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC204009090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\4627d3d8.pat(139):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0070 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR ?MakeSpace@D3D@@YGPCKXZ ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC204009090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\5344d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0070 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC204009090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\5558d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0068 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5659d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0068 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5788d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0068 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5849d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0068 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5933d3d8.pat(136):568B35........8B063B46047205E8........8B4C240885C9C7000803040075 0B F94C 0068 _D3DDevice_SetRenderState_CullMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^002BD _D3D__RenderState+024C ^003DD _D3D__RenderState+0248 ^0060D _D3D__RenderState+024C ........5EC20400C7400401000000578B3D........33D23BCF0F95C2C740089C03040083C0105F81C2040400008950FC8906890D........5EC20400
*/
		xbaddr pFunc = NULL;
		int iCodeOffsetFor_X_pDevice = 0x03; // verified for 3911, 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933
		int iCodeOffsetFor_X_D3DRS_CULLMODE = 0x2B; // verified for 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 5344)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_CullMode_5344, lower, upper);
		else if (g_BuildVersion >= 5233)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_CullMode_5233, lower, upper);
		else if (g_BuildVersion >= 4361)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_CullMode_4361, lower, upper);
		else if (g_BuildVersion >= 4034)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_CullMode_4034, lower, upper);
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_CullMode_3925, lower, upper);
			iCodeOffsetFor_X_D3DRS_CULLMODE = 0x25; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetRenderState_CullMode\n", pFunc);

			// Temporary verification - is XREF_D3DDEVICE derived correctly?
			// TODO : Remove this when D3DEVICE derivation is deemed stable
			CheckDerivedDevice(pFunc, iCodeOffsetFor_X_pDevice);

			CheckDerivedRenderState(pFunc, iCodeOffsetFor_X_D3DRS_CULLMODE, XREF_D3DRS_CULLMODE, X_D3DRS_CULLMODE);
		}
	}

	// Try to locate Xbox symbol "_D3D_RenderState" and store it's address (and a few derived)
	// via D3DDevice_SetRenderState_FillMode
	{
		extern LOOVPA<11> D3DDevice_SetRenderState_FillMode_3925;
		extern LOOVPA<7> D3DDevice_SetRenderState_FillMode_4034;
		extern LOOVPA<11> D3DDevice_SetRenderState_FillMode_4134;
/*
D:\Patrick\git\Dxbx\Resources\Patterns\3911d3d8.pat(123):568B35........56E8........8B0D........8B15........85C98B4C240875 16 17AA 0040 _D3DDevice_SetRenderState_FillMode@4 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0009R _XMETAL_StartPush@4 ^000FD _D3D__RenderState+01E4 ^0015D _D3D__RenderState+01E0 ^0036D _D3D__RenderState+01DC ........5EC204009090
D:\Patrick\git\Dxbx\Resources\Patterns\4361d3d8.pat(126):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0050 _D3DDevice_SetRenderState_FillMode@4 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^000FR ?MakeSpace@D3D@@YGPCKXZ ^0015D _D3D__RenderState+01E8 ^001BD _D3D__RenderState+01E4 ^003CD _D3D__RenderState+01E0 ........5EC20400909090909090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\4627d3d8.pat(147):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0050 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR ?MakeSpace@D3D@@YGPCKXZ ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400909090909090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\5344d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0050 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400909090909090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\5558d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0044 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5659d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0044 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5788d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0044 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5849d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0044 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400
D:\Patrick\git\Dxbx\Resources\Patterns\5933d3d8.pat(144):568B35........8B063B46047205E8........8B0D........8B15........85 1C 5B81 0044 _D3DDevice_SetRenderState_FillMode@4 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0015D _D3D__RenderState+0234 ^001BD _D3D__RenderState+0230 ^003CD _D3D__RenderState+022C ........5EC20400
*/
		xbaddr pFunc = NULL;
		int iCodeOffsetFor_X_pDevice = 0x03; // verified for 3911, 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933
		int iCodeOffsetFor_X_D3DRS_FILLMODE = 0x3C; // verified for 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 4134)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_FillMode_4134, lower, upper);
		else if (g_BuildVersion >= 4034)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_FillMode_4034, lower, upper);
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetRenderState_FillMode_3925, lower, upper);
			iCodeOffsetFor_X_D3DRS_FILLMODE = 0x36; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetRenderState_FillMode\n", pFunc);

			// Temporary verification - is XREF_D3DDEVICE derived correctly?
			// TODO : Remove this when D3DEVICE derivation is deemed stable
			CheckDerivedDevice(pFunc, iCodeOffsetFor_X_pDevice);

			CheckDerivedRenderState(pFunc, iCodeOffsetFor_X_D3DRS_FILLMODE, XREF_D3DRS_FILLMODE, X_D3DRS_FILLMODE);
		}
	}

	// Try to locate Xbox symbol "D3D__TextureState" and store it's address
	// via D3DDevice_SetTextureState_TexCoordIndex
	{
		extern LOOVPA<1 + 11> D3DDevice_SetTextureState_TexCoordIndex_3925;
		extern LOOVPA<1 + 10> D3DDevice_SetTextureState_TexCoordIndex_4034;
		extern LOOVPA<1 + 10> D3DDevice_SetTextureState_TexCoordIndex_4134;
		extern LOOVPA<1 + 10> D3DDevice_SetTextureState_TexCoordIndex_4361;
		extern LOOVPA<1 + 10> D3DDevice_SetTextureState_TexCoordIndex_4627;
/*
3911d3d8.pat(127):8B4C2408538B5C24088BC3C1E007558988........568B35........81E10000 87 DA9E 00E0 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0011D _D3D__TextureState+0070 ^0018D ?g_pDevice@D3D@@3PAVCDevice@1@A ^00A7R _XMETAL_StartPush@4 ........81C33CC0000089780489780889780CC1E304891883C01089068B46085F0D940100008946085E5D5BC2080090909090909090909090
4361d3d8.pat(130):538B5C2408558B6C2410568BC3C1E007578B3D........89A8........8B078B 11 1F57 0120 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0013D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0019D _D3D__TextureState+0070 ^0031R ?MakeSpace@D3D@@YGPCKXZ ^00A9D ?g_SlotMapping@D3D@@3PAEA+0009 ^00DAD _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ^010AD _D3D__DirtyFlags ........81E50000FFFFB901000000745E8D149D641904008910C74004000000FF83C00881FD00000300895C24187727741A81FD000001007407BE00240000EB2E894C2414BE11850000EB23894C2414BE12850000EB1881FD00000400740B894C2414BE02240000EB05BE012400008A54241880C2098893........8D933CC00000C1E204891089700489700889700C83C01089078B875404000085C08B442414750E85C0740A810D........000200008BAF540400008BD18BCBD3E2D3E0F7D223D50BD0899754040000A1........5F5E0D7F0400005DA3........5BC208009090909090909090909090909090
4627d3d8.pat(151):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0110 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R ?MakeSpace@D3D@@YGPCKXZ ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B861405000085C0750E85D2740A810D........000200008B9E140500008BC18BCFD3E0D3E25FF7D023C30BC2898614050000A1........5E0D7F0400005DA3........5BC208009090909090909090909090
5344d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0110 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B863805000085C0750E85D2740A810D........000200008B9E380500008BC18BCFD3E0D3E25FF7D023C30BC2898638050000A1........5E0D7F0400005DA3........5BC208009090909090909090909090
5558d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0105 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B864C09000085C0750E85D2740A810D........000200008B9E4C0900008BC18BCFD3E0D3E25FF7D023C30BC289864C090000A1........5E0D7F0400005DA3........5BC20800
5659d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0105 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B864C09000085C0750E85D2740A810D........000200008B9E4C0900008BC18BCFD3E0D3E25FF7D023C30BC289864C090000A1........5E0D7F0400005DA3........5BC20800
5788d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0105 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B865009000085C0750E85D2740A810D........000200008B9E500900008BC18BCFD3E0D3E25FF7D023C30BC2898650090000A1........5E0D7F0400005DA3........5BC20800
5849d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0105 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B865009000085C0750E85D2740A810D........000200008B9E500900008BC18BCFD3E0D3E25FF7D023C30BC2898650090000A1........5E0D7F0400005DA3........5BC20800
5933d3d8.pat(148):538B5C240C55568B35........578B7C24148BC7C1E0078998........8B068B 13 4281 0105 _D3DDevice_SetTextureState_TexCoordIndex@8 ^0009D _D3D__pDevice ^0019D _D3D__TextureState+0070 ^0033R _D3DDevice_MakeSpace@0 ^00A0D ?g_SlotMapping@D3D@@3PAEA+0009 ^00CDD _D3D__DirtyFlags ^00F1D _D3D__DirtyFlags ^00FDD _D3D__DirtyFlags ........8B54241481E30000FFFFB901000000744F8D2CBD641904008928C74004000000FF83C00881FB00000300897C2418772B741C81FB000001007407BD00240000EB1F894C2414BD118500008BD1EB12894C2414BD128500008BD1EB05BD012400008A5C241880C309889F........8D9F3CC00000C1E304891889680489680889680C83C01089068B865009000085C0750E85D2740A810D........000200008B9E500900008BC18BCFD3E0D3E25FF7D023C30BC2898650090000A1........5E0D7F0400005DA3........5BC20800
*/
		xbaddr pFunc = NULL;
		int iCodeOffsetFor_X_pDevice = 0x09; // verified for 4361
		int iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x19; // verified for 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 4627) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_TexCoordIndex_4627, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x09; // verified for 4627, 5344, 5558, 5659, 5788, 5849, 5933
		}
		else if (g_BuildVersion >= 4361)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_TexCoordIndex_4361, lower, upper);
		else if (g_BuildVersion >= 4134) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_TexCoordIndex_4134, lower, upper);
			iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x18; // unsure
		}
		else if (g_BuildVersion >= 4034) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_TexCoordIndex_4034, lower, upper);
			iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x18; // unsure
		}
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_TexCoordIndex_3925, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x11; // verified for 3911
			iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x11; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetTextureState_TexCoordIndex\n", pFunc);

			CheckDerivedDevice(pFunc, iCodeOffsetFor_X_pDevice);

			CheckDerivedTextureState(pFunc, iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX, XREF_D3DTSS_TEXCOORDINDEX, X_D3DTSS_TEXCOORDINDEX);
		}
	}

	// Try to locate Xbox symbol "D3D__TextureState" and store it's address
	// via D3DDevice_SetTextureState_BorderColor
	{
		extern LOOVPA<13> D3DDevice_SetTextureState_BorderColor_3925;
		extern LOOVPA<7> D3DDevice_SetTextureState_BorderColor_4034;
		extern LOOVPA<15> D3DDevice_SetTextureState_BorderColor_4361;
/*
D:\Patrick\git\Dxbx\Resources\Patterns\3911d3d8.pat(129):568B35........56E8........8B4C24088BD1C1E20681C2241B040089108B54 0F 270B 0040 _D3DDevice_SetTextureState_BorderColor@8 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^0009R _XMETAL_StartPush@4 ^002FD _D3D__TextureState+0074 ........5EC20800909090909090909090
D:\Patrick\git\Dxbx\Resources\Patterns\4361d3d8.pat(132):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 0040 _D3DDevice_SetTextureState_BorderColor@8 ^0003D ?g_pDevice@D3D@@3PAVCDevice@1@A ^000FR ?MakeSpace@D3D@@YGPCKXZ ^0035D _D3D__TextureState+0074 ........5EC20800909090
D:\Patrick\git\Dxbx\Resources\Patterns\4627d3d8.pat(153):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 0040 _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR ?MakeSpace@D3D@@YGPCKXZ ^0035D _D3D__TextureState+0074 ........5EC20800909090
D:\Patrick\git\Dxbx\Resources\Patterns\5344d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 0040 _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800909090
D:\Patrick\git\Dxbx\Resources\Patterns\5558d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 003D _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800
D:\Patrick\git\Dxbx\Resources\Patterns\5659d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 003D _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800
D:\Patrick\git\Dxbx\Resources\Patterns\5788d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 003D _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800
D:\Patrick\git\Dxbx\Resources\Patterns\5849d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 003D _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800
D:\Patrick\git\Dxbx\Resources\Patterns\5933d3d8.pat(150):568B35........8B063B46047205E8........8B4C24088BD1C1E20681C2241B 15 3141 003D _D3DDevice_SetTextureState_BorderColor@8 ^0003D _D3D__pDevice ^000FR _D3DDevice_MakeSpace@0 ^0035D _D3D__TextureState+0074 ........5EC20800
*/
		xbaddr pFunc = NULL;
		int iCodeOffsetFor_X_pDevice = 0x09; // verified for 4361
		int iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x19; // verified for 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 4361) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_BorderColor_4361, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x09; // verified for 4627, 5344, 5558, 5659, 5788, 5849, 5933
		}
		else if (g_BuildVersion >= 4034) {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_BorderColor_4034, lower, upper);
			iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x18; // unsure
		}
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetTextureState_BorderColor_3925, lower, upper);
			iCodeOffsetFor_X_pDevice = 0x11; // verified for 3911
			iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX = 0x11; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetTextureState_TexCoordIndex\n", pFunc);

			CheckDerivedDevice(pFunc, iCodeOffsetFor_X_pDevice);

			CheckDerivedTextureState(pFunc, iCodeOffsetFor_X_D3DTSS_TEXCOORDINDEX, XREF_D3DTSS_TEXCOORDINDEX, X_D3DTSS_TEXCOORDINDEX);
		}
	}

	// Locate Xbox symbol "g_Stream" and store it's address
	{
		extern LOOVPA<1 + 12> D3DDevice_SetStreamSource_3925;
		extern LOOVPA<1 + 14> D3DDevice_SetStreamSource_4034;

		xbaddr pFunc = NULL;
		int iCodeOffsetFor_g_Stream = 0x22; // verified for 4361, 4627, 5344, 5558, 5659, 5788, 5849, 5933

		if (g_BuildVersion >= 4034)
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetStreamSource_4034, lower, upper);
		else {
			pFunc = EmuLocateFunction((OOVPA*)&D3DDevice_SetStreamSource_3925, lower, upper);
			iCodeOffsetFor_g_Stream = 0x23; // verified for 3911
		}

		if (pFunc != NULL) {
			printf("HLE: Located 0x%.08X -> D3DDevice_SetStreamSource\n", pFunc);

			// Read address of Xbox_g_Stream(+8) from D3DDevice_SetStreamSource
			xbaddr Derived_g_Stream = *((xbaddr*)(pFunc + iCodeOffsetFor_g_Stream));

			// Temporary verification - is XREF_G_STREAM derived correctly?
			// TODO : Remove this when XREF_G_STREAM derivation is deemed stable
			if (VerifySymbolAddressAgainstXRef("g_Stream", Derived_g_Stream, XREF_G_STREAM)) {

				// Now that both Derived XREF and OOVPA-based function-contents match,
				// correct base-address (because "g_Stream" is actually "g_Stream"+8") :
				Derived_g_Stream -= offsetof(X_Stream, pVertexBuffer);
				g_SymbolAddresses["g_Stream"] = (xbaddr)Derived_g_Stream;
				printf("HLE: Derived 0x%.08X -> g_Stream\n", Derived_g_Stream);
			}
		}
	}
}

void EmuHLEIntercept(Xbe::Header *pXbeHeader)
{
	CheckHLEExports();

	Xbe::LibraryVersion *pLibraryVersion = (Xbe::LibraryVersion*)pXbeHeader->dwLibraryVersionsAddr;

    printf("\n");
	printf("*******************************************************************************\n");
	printf("* Cxbx-Reloaded High Level Emulation database last modified %s\n", szHLELastCompileTime);
	printf("*******************************************************************************\n");
	printf("\n");

	// Make sure the HLE Cache directory exists
	std::string cachePath = std::string(szFolder_CxbxReloadedData) + "\\HLECache\\";
	int result = SHCreateDirectoryEx(nullptr, cachePath.c_str(), nullptr);
	if ((result != ERROR_SUCCESS) && (result != ERROR_ALREADY_EXISTS)) {
		CxbxKrnlCleanup("Couldn't create Cxbx-Reloaded HLECache folder!");
	}

	// Hash the loaded XBE's header, use it as a filename
	uint32_t uiHash = XXHash32::hash((void*)&CxbxKrnl_Xbe->m_Header, sizeof(Xbe::Header), 0);
	std::stringstream sstream;
	sstream << cachePath << std::hex << uiHash << ".ini";
	std::string filename = sstream.str();

	if (PathFileExists(filename.c_str())) {
		printf("Found HLE Cache File: %08X.ini\n", uiHash);

		// Verify the version of the cache file against the HLE Database
		char buffer[SHRT_MAX] = { 0 };
		char* bufferPtr = buffer;

		GetPrivateProfileString("Info", "HLEDatabaseVersion", NULL, buffer, sizeof(buffer), filename.c_str());
		g_BuildVersion = GetPrivateProfileInt("Libs", "D3D8_BuildVersion", 0, filename.c_str());

		if (strcmp(buffer, szHLELastCompileTime) == 0) {
			printf("Using HLE Cache\n");
			GetPrivateProfileSection("Symbols", buffer, sizeof(buffer), filename.c_str());

			// Parse the .INI file into the map of symbol addresses
			while (strlen(bufferPtr) > 0) {
				std::string ini_entry(bufferPtr);

				auto separator = ini_entry.find('=');
				std::string key = ini_entry.substr(0, separator);
				std::string value = ini_entry.substr(separator + 1, std::string::npos);
				uint32_t addr = strtol(value.c_str(), 0, 16);

				g_SymbolAddresses[key] = addr;
				bufferPtr += strlen(bufferPtr) + 1;
			}

			// Iterate through the map of symbol addresses, calling GetEmuPatchAddr on all functions.	
			for (auto it = g_SymbolAddresses.begin(); it != g_SymbolAddresses.end(); ++it) {
				std::string functionName = (*it).first;
				xbaddr location = (*it).second;

				std::stringstream output;
				output << "HLECache: 0x" << std::setfill('0') << std::setw(8) << std::hex << location
					<< " -> " << functionName;
				void* pFunc = GetEmuPatchAddr(functionName);
				if (pFunc != nullptr)
				{
					// skip entries that weren't located at all
					if (location == NULL)
					{
						output << "\t(not patched)";
					}
					// Prevent patching illegal addresses
					else if (location < XBE_IMAGE_BASE)
					{
						output << "\t*ADDRESS TOO LOW!*";
					}
					else if (location > XBOX_MEMORY_SIZE)
					{
						output << "\t*ADDRESS TOO HIGH!*";
					}
					else
					{
						EmuInstallPatch(location, pFunc);
						output << "\t*PATCHED*";
					}
				}
				else
				{
					if (location != NULL)
						output << "\t(no patch)";
				}

				output << "\n";
				printf(output.str().c_str());
			}

			XTL::DxbxBuildRenderStateMappingTable();

			SetGlobalSymbols();

			XRefDataBase[XREF_D3DDEVICE] = g_SymbolAddresses["D3DDEVICE"];

			XTL::InitD3DDeferredStates();

			g_HLECacheUsed = true;
		}

		// If g_SymbolAddresses didn't get filled, the HLE cache is invalid
		if (g_SymbolAddresses.empty()) {
			printf("HLE Cache file is outdated and will be regenerated\n");
			g_HLECacheUsed = false;
		}
	}

	// If the HLE Cache was used, skip symbol searching/patching
	if (g_HLECacheUsed) {
		return;
	}

	//
    // initialize Microsoft XDK emulation
    //
    if(pLibraryVersion != 0)
    {
		printf("HLE: Detected Microsoft XDK application...\n");

		UnResolvedXRefs = XREF_COUNT; // = sizeof(XRefDataBase) / sizeof(xbaddr)

        uint32 dwLibraryVersions = pXbeHeader->dwLibraryVersions;
        uint32 LastUnResolvedXRefs = UnResolvedXRefs+1;
        uint32 OrigUnResolvedXRefs = UnResolvedXRefs;

		bool bFoundD3D = false;

		bXRefFirstPass = true; // Set to false for search speed optimization

		// Mark all Xrefs initially as undetermined
		memset((void*)XRefDataBase, XREF_ADDR_UNDETERMINED, sizeof(XRefDataBase));

		for(int p=0;UnResolvedXRefs < LastUnResolvedXRefs;p++)
        {
			printf("HLE: Starting pass #%d...\n", p+1);

            LastUnResolvedXRefs = UnResolvedXRefs;

            for(uint32 v=0;v<dwLibraryVersions;v++)
            {
                uint16 BuildVersion = pLibraryVersion[v].wBuildVersion;
                uint16 OrigBuildVersion = BuildVersion;

                // Aliases - for testing purposes only
				// TODO: Remove these and come up with a better way to handle XDKs we don't hve databases for
				if(BuildVersion == 4039) { BuildVersion = 4034; }
				if(BuildVersion == 4238) { BuildVersion = 4361; }	// I don't think this XDK was released.
				if(BuildVersion == 4242) { BuildVersion = 4361; }
				if(BuildVersion == 4400) { BuildVersion = 4361; }
				if(BuildVersion == 4531) { BuildVersion = 4432; }
                if(BuildVersion == 4721) { BuildVersion = 4627; }
				if(BuildVersion == 4831) { BuildVersion = 4627; }
                if(BuildVersion == 4928) { BuildVersion = 4627; }
                if(BuildVersion == 5455) { BuildVersion = 5558; }
                if(BuildVersion == 5659) { BuildVersion = 5558; }
				if(BuildVersion == 5120) { BuildVersion = 5233; }
                if(BuildVersion == 5933) { BuildVersion = 5849; }   // These XDK versions are pretty much the same
                
				std::string LibraryName(pLibraryVersion[v].szName, pLibraryVersion[v].szName + 8);
				
				// TODO: HACK: D3DX8 is packed into D3D8 database
				if (strcmp(LibraryName.c_str(), Lib_D3DX8) == 0)
				{
					LibraryName = Lib_D3D8;
				}

				if (strcmp(LibraryName.c_str(), Lib_D3D8LTCG) == 0)
				{
					// If LLE GPU is not enabled, show a warning that the title is not supported
					if (!bLLE_GPU)
						CxbxKrnlCleanup("LTCG Title Detected: This game is not supported by HLE");

					// Skip LTCG libraries as we cannot reliably detect them
					continue;
				}
				
				if (strcmp(LibraryName.c_str(), Lib_DSOUND) == 0)
                {
					// Skip scanning for DSOUND symbols when LLE APU is selected
					if (bLLE_APU)
						continue;

					// Several 3911 titles has different DSound builds.
					if(BuildVersion < 4034)
                    {
                        BuildVersion = 3936;
                    }

					// Redirect other highly similar DSOUND library versions
					if(BuildVersion == 4361 || BuildVersion == 4400 || BuildVersion == 4432 || 
						BuildVersion == 4531 )
						BuildVersion = 4627;
                }

				if (strcmp(LibraryName.c_str(), Lib_XAPILIB) == 0)
				{
					// Change a few XAPILIB versions to similar counterparts
					if(BuildVersion == 3944)
						BuildVersion = 3911;
					if(BuildVersion == 3950)
						BuildVersion = 3911;
					if(OrigBuildVersion == 4531)
						BuildVersion = 4627;
				}

				if (strcmp(LibraryName.c_str(), Lib_XGRAPHC) == 0)
				{
					// Skip scanning for XGRAPHC (XG) symbols when LLE GPU is selected
					if (bLLE_GPU)
						continue;

					if (BuildVersion == 3944)
						BuildVersion = 3911;
					if (OrigBuildVersion == 4531)
						BuildVersion = 4361;
				}

				if (strcmp(LibraryName.c_str(), Lib_D3D8) == 0)
				{
					// Skip scanning for D3D8 symbols when LLE GPU is selected
					if (bLLE_GPU)
						continue;

					// Prevent scanning D3D8 again (since D3D8X is packed into it above)
					if (bFoundD3D)
						continue;

					bFoundD3D = true;
					// Some 3911 titles have different D3D8 builds
					if (BuildVersion <= 3948)
						BuildVersion = 3925;
	
					if(bXRefFirstPass)
	                {
						using namespace XTL;

						// Save D3D8 build version
						g_BuildVersion = BuildVersion;

						PrescanD3D(pXbeHeader);
					}
                }

				printf("HLE: * Searching HLE database for %s version 1.0.%d... ", LibraryName.c_str(), BuildVersion);

                const HLEData *FoundHLEData = nullptr;
                for(uint32 d = 0; d < HLEDataBaseCount; d++) {
					if (BuildVersion == HLEDataBase[d].BuildVersion && strcmp(LibraryName.c_str(), HLEDataBase[d].Library) == 0) {
						FoundHLEData = &HLEDataBase[d];
						break;
					}
                }

				if (FoundHLEData) {
					if (g_bPrintfOn) printf("Found\n");
					EmuInstallPatches(FoundHLEData->OovpaTable, FoundHLEData->OovpaTableSize, pXbeHeader);
				} else {
					if (g_bPrintfOn) printf("Skipped\n");
				}
			}

            bXRefFirstPass = false;
        }

        // display Xref summary
		printf("HLE: Resolved %d cross reference(s)\n", OrigUnResolvedXRefs - UnResolvedXRefs);
    }

	printf("\n");

#ifndef PATCH_TEXTURES
	g_SymbolAddresses["offsetof(D3DDevice,m_Textures)"] = XRefDataBase[XREF_OFFSET_D3DDEVICE_M_TEXTURES];
#endif

	SetGlobalSymbols();

	XTL::InitD3DDeferredStates();

	// Write the HLE Database version string
	WritePrivateProfileString("Info", "HLEDatabaseVersion", szHLELastCompileTime, filename.c_str());

	// Write the Certificate Details to the cache file
	char tAsciiTitle[40] = "Unknown";
	setlocale(LC_ALL, "English");
	wcstombs(tAsciiTitle, g_pCertificate->wszTitleName, sizeof(tAsciiTitle));
	WritePrivateProfileString("Certificate", "Name", tAsciiTitle, filename.c_str());

	std::stringstream titleId;
	titleId << std::hex << g_pCertificate->dwTitleId;
	WritePrivateProfileString("Certificate", "TitleID", titleId.str().c_str(), filename.c_str());

	std::stringstream region;
	region << std::hex << g_pCertificate->dwGameRegion;
	WritePrivateProfileString("Certificate", "Region", region.str().c_str(), filename.c_str());

	// Write Library Details
	for (uint i = 0; i < pXbeHeader->dwLibraryVersions; i++)	{
		std::string LibraryName(pLibraryVersion[i].szName, pLibraryVersion[i].szName + 8);
		std::stringstream buildVersion;
		buildVersion << pLibraryVersion[i].wBuildVersion;

		WritePrivateProfileString("Libs", LibraryName.c_str(), buildVersion.str().c_str(), filename.c_str());
	}

	std::stringstream buildVersion;
	buildVersion << g_BuildVersion;
	WritePrivateProfileString("Libs", "D3D8_BuildVersion", buildVersion.str().c_str(), filename.c_str());

	// Write the found symbol addresses into the cache file
	for(auto it = g_SymbolAddresses.begin(); it != g_SymbolAddresses.end(); ++it) {
		std::stringstream cacheAddress;
		cacheAddress << std::hex << (*it).second;
		WritePrivateProfileString("Symbols", (*it).first.c_str(), cacheAddress.str().c_str(), filename.c_str());
	}

    return;
}

static inline void EmuInstallPatch(xbaddr FunctionAddr, void *Patch)
{
    uint08 *FuncBytes = (uint08*)FunctionAddr;

	*(uint08*)&FuncBytes[0] = OPCODE_JMP_E9; // = opcode for JMP rel32 (Jump near, relative, displacement relative to next instruction)
    *(uint32*)&FuncBytes[1] = (uint32)Patch - FunctionAddr - 5;
}

static inline void GetXRefEntry(OOVPA *oovpa, int index, OUT uint32 &xref, OUT uint08 &offset)
{
	// Note : These are stored swapped by the XREF_ENTRY macro, hence this difference from GetOovpaEntry :
	xref = (uint32)((LOOVPA<1>*)oovpa)->Lovp[index].Offset;
	offset = ((LOOVPA<1>*)oovpa)->Lovp[index].Value;
}

static inline void GetOovpaEntry(OOVPA *oovpa, int index, OUT uint32 &offset, OUT uint08 &value)
{
	offset = (uint32)((LOOVPA<1>*)oovpa)->Lovp[index].Offset;
	value = ((LOOVPA<1>*)oovpa)->Lovp[index].Value;
}

static boolean CompareOOVPAToAddress(OOVPA *Oovpa, xbaddr cur)
{
	// NOTE : Checking offsets uses bytes. Doing that first is probably
	// faster than first checking (more complex) xrefs.

	// Check all (Offset,Value)-pairs, stop if any does not match
	for (uint32 v = Oovpa->XRefCount; v < Oovpa->Count; v++)
	{
		uint32 Offset;
		uint08 ExpectedValue;

		// get offset + value pair
		GetOovpaEntry(Oovpa, v, Offset, ExpectedValue);
		uint08 ActualValue = *(uint08*)(cur + Offset);
		if (ActualValue != ExpectedValue)
			return false;
	}

	// Check all XRefs, stop if any does not match
	for (uint32 v = 0; v < Oovpa->XRefCount; v++)
	{
		uint32 XRef;
		uint08 Offset;

		// get currently registered (un)known address
		GetXRefEntry(Oovpa, v, XRef, Offset);
		xbaddr XRefAddr = XRefDataBase[XRef];
		// Undetermined XRef cannot be checked yet
		// (EmuLocateFunction already checked this, but this check
		// is cheap enough to keep, and keep this function generic).
		if (XRefAddr == XREF_ADDR_UNDETERMINED)
			return false;

		// Don't verify an xref that has to be (but isn't yet) derived
		if (XRefAddr == XREF_ADDR_DERIVE)
			continue;

		xbaddr ActualAddr = *(xbaddr*)(cur + Offset);
		// check if direct reference matches XRef
		if (ActualAddr != XRefAddr)
			// check if PC-relative matches XRef
			if (ActualAddr + cur + Offset + 4 != XRefAddr)
				return false;
	}

	// all offsets matched
	return true;
}

// locate the given function, searching within lower and upper bounds
static xbaddr EmuLocateFunction(OOVPA *Oovpa, xbaddr lower, xbaddr upper)
{
	// skip out if this is an unnecessary search
	if (!bXRefFirstPass && Oovpa->XRefCount == XRefZero && Oovpa->XRefSaveIndex == XRefNoSaveIndex)
		return (xbaddr)nullptr;

	uint32_t derive_indices = 0;
	// Check all XRefs are known (if not, don't do a useless scan) :
	for (uint32 v = 0; v < Oovpa->XRefCount; v++)
	{
		uint32 XRef;
		uint08 Offset;

		// get currently registered (un)known address
		GetXRefEntry(Oovpa, v, XRef, Offset);
		xbaddr XRefAddr = XRefDataBase[XRef];
		// Undetermined XRef cannot be checked yet
		if (XRefAddr == XREF_ADDR_UNDETERMINED)
			// Skip this scan over the address range
			return (xbaddr)nullptr;

		// Don't verify an xref that has to be (but isn't yet) derived
		if (XRefAddr == XREF_ADDR_DERIVE)
		{
			// Mark (up to index 32) which xref needs to be derived
			derive_indices |= (1 << v);
			continue;
		}
	}

	// correct upper bound with highest Oovpa offset
	uint32 count = Oovpa->Count;
	{
		uint32 Offset;
		uint08 Value; // ignored

		GetOovpaEntry(Oovpa, count - 1, Offset, Value);
		upper -= Offset;
	}

	// search all of the image memory
	for (xbaddr cur = lower; cur < upper; cur++)
		if (CompareOOVPAToAddress(Oovpa, cur))
		{
			// do we need to save the found address?
			if (Oovpa->XRefSaveIndex != XRefNoSaveIndex)
			{
				// is the XRef not saved yet?
				switch (XRefDataBase[Oovpa->XRefSaveIndex]) {
				case XREF_ADDR_NOT_FOUND:
				{
					EmuWarning("Found OOVPA after first finding nothing?");
					// fallthrough to XREF_ADDR_UNDETERMINED
				}
				case XREF_ADDR_UNDETERMINED:
				{
					// save and count the found address
					UnResolvedXRefs--;
					XRefDataBase[Oovpa->XRefSaveIndex] = cur;
					break;
				}
				case XREF_ADDR_DERIVE:
				{
					EmuWarning("Cannot derive a save index!");
					break;
				}
				default:
				{
					if (XRefDataBase[Oovpa->XRefSaveIndex] != cur)
						EmuWarning("Found OOVPA on other address than in XRefDataBase!");
					break;
				}
				}
			}

			while (derive_indices > 0)
			{
				uint32 XRef;
				uint08 Offset;
				DWORD derive_index;

				// Extract an index from the indices mask :
				_BitScanReverse(&derive_index, derive_indices); // MSVC intrinsic; GCC has __builtin_clz
				derive_indices ^= (1 << derive_index);

				// get currently registered (un)known address
				GetXRefEntry(Oovpa, derive_index, XRef, Offset);

				// Calculate the address where the XRef resides
				xbaddr XRefAddr = cur + Offset;
				// Read the address it points to
				XRefAddr = *((xbaddr*)XRefAddr);

				/* For now assume it's a direct reference;
				// TODO : Check if it's PC-relative reference?
				if (XRefAddr + cur + Offset + 4 < XBE_MAX_VA)
					XRefAddr = XRefAddr + cur + Offset + 4;
				*/

				// Does the address seem valid?
				if (XRefAddr < XBE_MAX_VA)
				{
					// save and count the derived address
					UnResolvedXRefs--;
					XRefDataBase[XRef] = XRefAddr;
					printf("HLE: Derived XREF(%d) -> 0x%0.8X (read from 0x%.08X+0x%X)\n", XRef, XRefAddr, cur, Offset);
				}
			}

			return cur;
		}

	// found nothing
    return (xbaddr)nullptr;
}

// install function interception wrappers
static void EmuInstallPatches(OOVPATable *OovpaTable, uint32 OovpaTableSize, Xbe::Header *pXbeHeader)
{
    xbaddr lower = pXbeHeader->dwBaseAddr;

	// Find the highest address contained within an executable segment
	xbaddr upper = pXbeHeader->dwBaseAddr;
	Xbe::SectionHeader* headers = reinterpret_cast<Xbe::SectionHeader*>(pXbeHeader->dwSectionHeadersAddr);

	for (uint32_t i = 0; i < pXbeHeader->dwSections; i++) {
		xbaddr end_addr = headers[i].dwVirtualAddr + headers[i].dwVirtualSize;
		if (headers[i].dwFlags.bExecutable && end_addr > upper) {
			upper = end_addr;
		}	
	}

    // traverse the full OOVPA table
    for(size_t a=0;a<OovpaTableSize/sizeof(OOVPATable);a++)
    {
		// Never used : skip scans when so configured
		bool DontScan = (OovpaTable[a].Flags & Flag_DontScan) > 0;
		if (DontScan)
			continue;

		// Skip already found & handled symbols
		xbaddr pFunc = g_SymbolAddresses[OovpaTable[a].szFuncName];
		if (pFunc != (xbaddr)nullptr)
			continue;

		// Search for each function's location using the OOVPA
        OOVPA *Oovpa = OovpaTable[a].Oovpa;
		pFunc = (xbaddr)EmuLocateFunction(Oovpa, lower, upper);
		if (pFunc == (xbaddr)nullptr)
			continue;

		// Now that we found the address, store it (regardless if we patch it or not)
		g_SymbolAddresses[OovpaTable[a].szFuncName] = (uint32_t)pFunc;

		// Output some details
		std::stringstream output;
		output << "HLE: 0x" << std::setfill('0') << std::setw(8) << std::hex << pFunc
			<< " -> " << OovpaTable[a].szFuncName << " " << std::dec << OovpaTable[a].Version;

		bool IsXRef = (OovpaTable[a].Flags & Flag_XRef) > 0;
		if (IsXRef)
			output << "\t(XREF)";

		// Retrieve the associated patch, if any is available
		void* addr = GetEmuPatchAddr(std::string(OovpaTable[a].szFuncName));
		bool DontPatch = (OovpaTable[a].Flags & Flag_DontPatch) > 0;
		if (DontPatch)
		{
			// Mention if there's an unused patch
			if (addr != nullptr)
				output << "\t*PATCH UNUSED!*";
			else
				output << "\t*DISABLED*";
		}
		else
		{
			if (addr != nullptr)
			{
				EmuInstallPatch(pFunc, addr);
				output << "\t*PATCHED*";
			}
			else
			{
				// Mention there's no patch available, if it was to be applied
				if (!IsXRef) // TODO : Remove this restriction once we patch xrefs regularly
					output << "\t*NO PATCH AVAILABLE!*";
			}
		}

		output << "\n";
		printf(output.str().c_str());
	}
}

#ifdef _DEBUG_TRACE

struct HLEVerifyContext {
	const HLEData *main_data;
	OOVPA *oovpa, *against;
	const HLEData *against_data;
	uint32 main_index, against_index;
};

std::string HLEErrorString(const HLEData *data, uint32 index)
{
	std::string result = 
		"OOVPATable " + (std::string)(data->Library) + "_" + std::to_string(data->BuildVersion)
		+ "[" + std::to_string(index) + "] " 
		+ (std::string)(data->OovpaTable[index].szFuncName);

	return result;
}

void HLEError(HLEVerifyContext *context, char *format, ...)
{
	std::string output = "HLE Error ";
	if (context->main_data != nullptr)
		output += "in " + HLEErrorString(context->main_data, context->main_index);

	if (context->against != nullptr && context->against_data != nullptr)
		output += ", comparing against " + HLEErrorString(context->against_data, context->against_index);

	// format specific error message
	char buffer[200];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	output += " : " + (std::string)buffer + (std::string)"\n";
	printf(output.c_str());
}

void VerifyHLEDataBaseAgainst(HLEVerifyContext *context); // forward

void VerifyHLEOOVPA(HLEVerifyContext *context, OOVPA *oovpa)
{
	if (context->against == nullptr) {
		// TODO : verify XRefSaveIndex and XRef's (how?)

		// verify offsets are in increasing order
		uint32 prev_offset;
		uint08 dummy_value;
		GetOovpaEntry(oovpa, oovpa->XRefCount, prev_offset, dummy_value);
		for (int p = oovpa->XRefCount + 1; p < oovpa->Count; p++) {
			uint32 curr_offset;
			GetOovpaEntry(oovpa, p, curr_offset, dummy_value);
			if (!(curr_offset > prev_offset)) {
				HLEError(context, "Lovp[%d] : Offset (0x%x) must be larger then previous offset (0x%x)",
					p, curr_offset, prev_offset);
			}
		}

		// find duplicate OOVPA's across all other data-table-oovpa's
		context->oovpa = oovpa;
		context->against = oovpa;
		VerifyHLEDataBaseAgainst(context);
		context->against = nullptr; // reset scanning state
		return;
	}

	// prevent checking an oovpa against itself
	if (context->against == oovpa)
		return;

	// compare {Offset, Value}-pairs between two OOVPA's
	OOVPA *left = context->against, *right = oovpa;
	int l = 0, r = 0;
	uint32 left_offset, right_offset;
	uint08 left_value, right_value;
	GetOovpaEntry(left, l, left_offset, left_value);
	GetOovpaEntry(right, r, right_offset, right_value);
	int unique_offset_left = 0;
	int unique_offset_right = 0;
	int equal_offset_value = 0;
	int equal_offset_different_value = 0;
	while (true) {
		bool left_next = true;
		bool right_next = true;

		if (left_offset < right_offset) {
			unique_offset_left++;
			right_next = false;
		}
		else if (left_offset > right_offset) {
			unique_offset_right++;
			left_next = false;
		}
		else if (left_value == right_value) {
			equal_offset_value++;
		}
		else {
			equal_offset_different_value++;
		}

		// increment r before use (in left_next)
		if (right_next)
			r++;

		if (left_next) {
			l++;
			if (l >= left->Count) {
				unique_offset_right += right->Count - r;
				break;
			}

			GetOovpaEntry(left, l, left_offset, left_value);
		}

		if (right_next) {
			if (r >= right->Count) {
				unique_offset_left += left->Count - l;
				break;
			}

			GetOovpaEntry(right, r, right_offset, right_value);
		}
	}

	// no mismatching values on identical offsets?
	if (equal_offset_different_value == 0)
		// enough matching OV-pairs?
		if (equal_offset_value > 4)
		{
			// no unique OV-pairs on either side?
			if (unique_offset_left + unique_offset_right == 0)
				HLEError(context, "OOVPA's are identical",
					unique_offset_left,
					unique_offset_right);
			else
				// not too many new OV-pairs on the left side?
				if (unique_offset_left < 6)
					// not too many new OV-parirs on the right side?
					if (unique_offset_right < 6)
						HLEError(context, "OOVPA's are expanded (left +%d, right +%d)",
							unique_offset_left,
							unique_offset_right);
		}
}

void VerifyHLEDataEntry(HLEVerifyContext *context, const OOVPATable *table, uint32 index, uint32 count)
{
	if (context->against == nullptr) {
		context->main_index = index;
	} else {
		context->against_index = index;
	}

	if (context->against == nullptr) {
		if (table[index].Flags & Flag_DontPatch)
		{
			if (GetEmuPatchAddr((std::string)table[index].szFuncName))
			{
				HLEError(context, "OOVPA registration DISABLED while a patch exists!");
			}
		}
		else
		if (table[index].Flags & Flag_XRef)
		{
			if (GetEmuPatchAddr((std::string)table[index].szFuncName))
			{
				HLEError(context, "OOVPA registration XREF while a patch exists!");
			}
		}
	}

	// verify the OOVPA of this entry
	if (table[index].Oovpa != nullptr)
		VerifyHLEOOVPA(context, table[index].Oovpa);
}

void VerifyHLEData(HLEVerifyContext *context, const HLEData *data)
{
	if (context->against == nullptr) {
		context->main_data = data;
	} else {
		context->against_data = data;
	}

	// Don't check a database against itself :
	if (context->main_data == context->against_data)
		return;

	// verify each entry in this HLEData
	uint32 count = data->OovpaTableSize / sizeof(OOVPATable);
	for (uint32 e = 0; e < count; e++)
		VerifyHLEDataEntry(context, data->OovpaTable, e, count);
}

void VerifyHLEDataBaseAgainst(HLEVerifyContext *context)
{
	// verify all HLEData's
	for (uint32 d = 0; d < HLEDataBaseCount; d++)
		VerifyHLEData(context, &HLEDataBase[d]);
}

void VerifyHLEDataBase()
{
	HLEVerifyContext context = { 0 };
	VerifyHLEDataBaseAgainst(&context);
}
#endif // _DEBUG_TRACE
