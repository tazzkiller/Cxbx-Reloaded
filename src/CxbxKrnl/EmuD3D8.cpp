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
// *   Cxbx->Win32->CxbxKrnl->EmuD3D8.cpp
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
#define _XBOXKRNL_DEFEXTRN_

#define LOG_PREFIX "D3D8"

#include "xxhash32.h"
#include <condition_variable>

// prevent name collisions
namespace xboxkrnl
{
    #include <xboxkrnl/xboxkrnl.h>
};

#include "CxbxUtil.h"
#include "CxbxVersion.h"
#include "CxbxKrnl.h"
#include "Emu.h"
#include "EmuFS.h"
#include "EmuShared.h"
#include "DbgConsole.h"
#include "ResourceTracker.h" // For g_VBTrackTotal, g_bVBSkipStream //, g_AlignCache
#include "MemoryManager.h"
#include "EmuXTL.h"
#include "HLEDatabase.h"
#include "Logging.h"
#include "EmuD3D8Logging.h"
#include "HLEIntercept.h" // for bLLE_GPU
#include "EmuD3D8\TextureCache.h"

#include <assert.h>
#include <process.h>
#include <clocale>

// This doesn't work : #include <dxerr8.h> // See DXGetErrorString8A below

#define MAX_CACHE_SIZE_INDEXBUFFERS 256
#define MAX_CACHE_SIZE_VERTEXBUFFERS 256

// Global(s)
HWND                                g_hEmuWindow   = nullptr; // rendering window
XTL::LPDIRECT3DDEVICE8              g_pD3DDevice8  = nullptr; // Direct3D8 Device
XTL::LPDIRECTDRAWSURFACE7           g_pDDSPrimary7 = nullptr; // DirectDraw7 Primary Surface
XTL::LPDIRECTDRAWSURFACE7           g_pDDSOverlay7 = nullptr; // DirectDraw7 Overlay Surface
XTL::LPDIRECTDRAWCLIPPER            g_pDDClipper   = nullptr; // DirectDraw7 Clipper
DWORD                               g_CurrentVertexShader = 0;
//DWORD								g_dwCurrentPixelShader = 0;
//XTL::CxbxPixelShader             *g_CurrentPixelShader = nullptr;
//BOOL                              g_bFakePixelShaderLoaded = FALSE;
BOOL                                g_bIsFauxFullscreen = FALSE;
BOOL								g_bHackUpdateSoftwareOverlay = FALSE;

// Static Function(s)
static BOOL WINAPI                  EmuEnumDisplayDevices(GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm);
static DWORD WINAPI                 EmuRenderWindow(LPVOID);
static DWORD WINAPI                 EmuCreateDeviceProxy(LPVOID);
static LRESULT WINAPI               EmuMsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static DWORD WINAPI                 EmuUpdateTickCount(LPVOID);
#if 0
static void                         CxbxUpdateResource(XTL::X_D3DResource *pResource);
static void                         EmuAdjustPower2(UINT *dwWidth, UINT *dwHeight);
#endif
static void							UpdateCurrentMSpFAndFPS(); // Used for benchmarking/fps count

// Static Variable(s)
static HMONITOR                     g_hMonitor      = NULL; // Handle to DirectDraw monitor
static BOOL                         g_bSupportsYUY2Overlay = FALSE; // Does device support YUY2 overlays?
static XTL::LPDIRECTDRAW7           g_pDD7          = NULL; // DirectDraw7
static XTL::DDCAPS                  g_DriverCaps          = { 0 };
#if 0
static DWORD                        g_dwOverlayW    = 640;  // Cached Overlay Width
static DWORD                        g_dwOverlayH    = 480;  // Cached Overlay Height
static DWORD                        g_dwOverlayP    = 640;  // Cached Overlay Pitch
#endif
static Xbe::Header                 *g_XbeHeader     = NULL; // XbeHeader
static uint32                       g_XbeHeaderSize = 0;    // XbeHeaderSize
static HBRUSH                       g_hBgBrush      = NULL; // Background Brush
static volatile bool                g_bRenderWindowActive = false;
static XBVideo                      g_XBVideo;
static XTL::X_D3DVBLANKCALLBACK     g_pVBCallback   = NULL; // Vertical-Blank callback routine
static std::condition_variable		g_VBConditionVariable;	// Used in BlockUntilVerticalBlank
static std::mutex					g_VBConditionMutex;		// Used in BlockUntilVerticalBlank
static XTL::X_D3DSWAPCALLBACK		g_pSwapCallback = NULL;	// Swap/Present callback routine
static XTL::X_D3DCALLBACK			g_pCallback		= NULL;	// D3DDevice::InsertCallback routine
static XTL::X_D3DCALLBACKTYPE		g_CallbackType;			// Callback type
static DWORD						g_CallbackParam;		// Callback param
static BOOL                         g_bHasDepthBits = FALSE;    // Has the Depth/Stencil surface a depth component?
static BOOL                         g_bHasStencilBits = FALSE;  // Has the Depth/Stencil surface a stencil component?
static clock_t						g_DeltaTime = 0;			 // Used for benchmarking/fps count
static unsigned int					g_Frames = 0;				 // Used for benchmarking/fps count
static DWORD						g_dwPrimPerFrame = 0;	// Number of primitives within one frame

// D3D based variables
static GUID                         g_ddguid;               // DirectDraw driver GUID
static XTL::LPDIRECT3D8             g_pD3D8 = nullptr;		// Direct3D8
static XTL::D3DDEVTYPE				g_D3DDeviceType;		// Direct3D8 Device type
static XTL::D3DCAPS8                g_D3DCaps;              // Direct3D8 Caps
static XTL::D3DDISPLAYMODE          g_D3DDisplayMode = {};  // Direct3D8 Adapter Display mode
static XTL::X_D3DPRESENT_PARAMETERS XboxD3DPresentParameters = {};


// Modifieable, behaviour-changing variable
int                                 g_FillModeOverride = 0; // Default 0=Xbox pass-through, 1=D3DFILL_WIREFRAME, 2=D3DFILL_POINT, 3=D3DFILL_SOLID
int                                 X_D3DSCM_CORRECTION_VersionDependent = 0; // version-dependent correction on shader constant numbers

#if 0
// current active index buffer
static XTL::X_D3DIndexBuffer       *g_pIndexBuffer  = NULL; // current active index buffer
static DWORD                        g_dwBaseVertexIndex = 0;// current active index buffer base index
#endif

// current active vertex stream
static XTL::IDirect3DVertexBuffer8 *g_pDummyBuffer = nullptr;  // Dummy buffer, used to set unused stream sources with

// current vertical blank information
static XTL::X_D3DVBLANKDATA         g_VBData = {0};
static DWORD                        g_VBLastSwap = 0;

// current swap information
static XTL::X_D3DSWAPDATA			g_SwapData = {0};
#if 0
static DWORD						g_SwapLast = 0;
#endif

#if 0
static XTL::X_D3DMATERIAL           g_BackMaterial = { 0 };
#endif

// cached Direct3D state variable(s)
#ifndef USE_XBOX_BUFFERS
static XTL::X_D3DSurface           *g_pInitialXboxBackBuffer = NULL;
static XTL::IDirect3DSurface8      *g_pInitialHostBackBuffer = nullptr;
static XTL::X_D3DSurface           *g_pActiveXboxBackBuffer = NULL;
#endif
static XTL::IDirect3DSurface8      *g_pActiveHostBackBuffer = nullptr;

#ifndef USE_XBOX_BUFFERS
static XTL::X_D3DSurface           *g_pInitialXboxRenderTarget = NULL;
static XTL::IDirect3DSurface8      *g_pInitialHostRenderTarget = nullptr;
static XTL::X_D3DSurface           *g_pActiveXboxRenderTarget = NULL;
static XTL::IDirect3DSurface8      *g_pActiveHostRenderTarget = nullptr;

static XTL::X_D3DSurface           *g_pInitialXboxDepthStencil = NULL;
static XTL::IDirect3DSurface8      *g_pInitialHostDepthStencil = nullptr;
static XTL::X_D3DSurface           *g_pActiveXboxDepthStencil = NULL;
static XTL::IDirect3DSurface8      *g_pActiveHostDepthStencil = nullptr;
#endif

static XTL::IDirect3DIndexBuffer8  *pClosingLineLoopIndexBuffer = nullptr;

static XTL::IDirect3DIndexBuffer8  *pQuadToTriangleD3DIndexBuffer = nullptr;
static UINT                         QuadToTriangleD3DIndexBuffer_Size = 0; // = NrOfQuadVertices

static XTL::INDEX16                *pQuadToTriangleIndexBuffer = nullptr;
static UINT                         QuadToTriangleIndexBuffer_Size = 0; // = NrOfQuadVertices

// TODO : Read this from Xbox, so [Get|Set][RenderTarget|DepthStencil] patches can be disabled
XTL::X_D3DSurface *GetXboxRenderTarget()
{
#ifdef USE_XBOX_BUFFERS
	return XTL::Xbox_D3DDevice_m_pRenderTarget ? *XTL::Xbox_D3DDevice_m_pRenderTarget : NULL;
#else
	return g_pInitialXboxRenderTarget;
#endif
}

XTL::X_D3DSurface *GetXboxDepthStencil()
{
#ifdef USE_XBOX_BUFFERS
	return XTL::Xbox_D3DDevice_m_pDepthStencil ? *XTL::Xbox_D3DDevice_m_pDepthStencil : NULL;
#else
	return g_pActiveXboxDepthStencil;
#endif
}

#if 0
static XTL::X_D3DSurface           *g_pCachedYuvSurface = NULL;
#endif

static DWORD                        g_dwVertexShaderUsage = 0;
static DWORD                        g_VertexShaderSlots[D3DVS_XBOX_NR_ADDRESS_SLOTS];

#if 0
// cached palette pointer
static DWORD *g_pTexturePaletteStages[X_D3DTSS_STAGECOUNT] = { NULL, NULL, NULL, NULL };
#endif

static XTL::X_D3DSHADERCONSTANTMODE g_VertexShaderConstantMode = X_D3DSCM_192CONSTANTS;

// cached Direct3D tiles
XTL::X_D3DTILE XTL::EmuD3DTileCache[0x08] = {0};

// Xbox D3DDevice related variables
xbaddr XTL::Xbox_pD3DDevice = NULL; // The address where an Xbe will put it's D3DDevice pointer
DWORD *XTL::Xbox_D3DDevice = NULL; // Once known, the actual D3DDevice pointer (see DeriveXboxD3DDeviceAddresses)
DWORD *XTL::Xbox_D3DDevice_m_IndexBase = NULL;
uint XTL::offsetof_Xbox_D3DDevice_m_pTextures = 0;
XTL::X_D3DBaseTexture **XTL::Xbox_D3DDevice_m_pTextures = NULL; // Can only be set once Xbox CreateDevice has ran
uint XTL::offsetof_Xbox_D3DDevice_m_pPalettes = 0;
XTL::X_D3DPalette **XTL::Xbox_D3DDevice_m_pPalettes = NULL; // Can only be set once Xbox CreateDevice has ran
uint XTL::offsetof_Xbox_D3DDevice_m_PixelShader = 0;
XTL::DWORD *XTL::Xbox_D3DDevice_m_PixelShader = NULL;
uint XTL::offsetof_Xbox_D3DDevice_m_pRenderTarget = 0;
XTL::X_D3DSurface **XTL::Xbox_D3DDevice_m_pRenderTarget = NULL;
uint XTL::offsetof_Xbox_D3DDevice_m_pDepthStencil = 0;
XTL::X_D3DSurface **XTL::Xbox_D3DDevice_m_pDepthStencil = NULL;
static TextureCache g_TextureCache; // TODO : Move to own file?

// Forward function declarations
const char *D3DErrorString(HRESULT hResult); // forward
XTL::X_D3DSurface *EmuNewD3DSurface(); // forward
void ConvertHostSurfaceHeaderToXbox
(
	XTL::IDirect3DSurface8 *pHostSurface,
	XTL::X_D3DPixelContainer* pPixelContainer
); // forward
void UpdateDepthStencilFlags(const XTL::X_D3DSurface *pXboxSurface); // forward
void Cxbx_Direct3D_CreateDevice(); // forward


#ifdef _DEBUG_TRACE

#define DEBUG_D3DRESULT(hRet, message) \
	do { \
		CXBX_CHECK_INTEGRITY(); \
		if (FAILED(hRet)) \
			if(g_bPrintfOn) \
				printf("%s : %s D3D error (0x%.08X: %s)\n", _logFuncPrefix.c_str(), message, hRet, D3DErrorString(hRet)); \
	} while (0)

#else

#define DEBUG_D3DRESULT(hRet, message) \
	do { \
		if (FAILED(hRet)) \
			if(g_bPrintfOn) \
				DbgPrintf("%s : %s D3D error (0x%.08X: %s)\n", __func__, message, hRet, D3DErrorString(hRet)); \
	} while (0)

#endif

void CxbxInitD3DState()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	HRESULT hRet;
#ifndef USE_XBOX_BUFFERS
	// HACK : Try to map Xbox to Host for : BackBuffer, RenderTarget and DepthStencil
	// TODO : This doesn't really help, and introduces host-allocated resources into
	// Xbox-land - it would be (much) better if we could run Xbox CreateDevice unpatched!
	{
		// Init backbuffer
		hRet = g_pD3DDevice8->GetBackBuffer(0, XTL::D3DBACKBUFFER_TYPE_MONO, &g_pInitialHostBackBuffer);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetRenderTarget");

		if (SUCCEEDED(hRet)) {
			g_pInitialXboxBackBuffer = EmuNewD3DSurface();
			if (g_pInitialHostBackBuffer != nullptr) {
				ConvertHostSurfaceHeaderToXbox(g_pInitialHostBackBuffer, g_pInitialXboxBackBuffer);

				//SetHostSurface(g_pInitialXboxBackBuffer, g_pInitialHostBackBuffer);
				g_pInitialHostBackBuffer->Release();
			}
		}

		g_pActiveXboxBackBuffer = g_pInitialXboxBackBuffer;

		// update render target cache
		hRet = g_pD3DDevice8->GetRenderTarget(&g_pInitialHostRenderTarget);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetRenderTarget");

		if (SUCCEEDED(hRet)) {
			g_pInitialXboxRenderTarget = EmuNewD3DSurface();
			if (g_pInitialHostRenderTarget != nullptr) {
				ConvertHostSurfaceHeaderToXbox(g_pInitialHostRenderTarget, g_pInitialXboxRenderTarget);

				XTL::D3DLOCKED_RECT LockedRect;
				g_pInitialHostRenderTarget->LockRect(&LockedRect, nullptr, 0);
				g_pInitialXboxRenderTarget->Data = (DWORD)LockedRect.pBits;// Was CXBX_D3DRESOURCE_DATA_RENDER_TARGET;

				//SetHostSurface(g_pInitialXboxRenderTarget, g_pInitialHostRenderTarget);
				g_pInitialHostRenderTarget->Release();
			}
		}

		g_pActiveXboxRenderTarget = g_pInitialXboxRenderTarget;

		// update z-stencil surface cache
		hRet = g_pD3DDevice8->GetDepthStencilSurface(&g_pInitialHostDepthStencil);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetDepthStencilSurface");

		if (SUCCEEDED(hRet)) {
			g_pInitialXboxDepthStencil = EmuNewD3DSurface();
			if (g_pInitialXboxDepthStencil != nullptr) {
				ConvertHostSurfaceHeaderToXbox(g_pInitialHostDepthStencil, g_pInitialXboxDepthStencil);

				XTL::D3DLOCKED_RECT LockedRect;
				g_pInitialHostDepthStencil->LockRect(&LockedRect, nullptr, 0);
				g_pInitialXboxDepthStencil->Data = (DWORD)LockedRect.pBits; // Was CXBX_D3DRESOURCE_DATA_DEPTH_STENCIL;

				//SetHostSurface(g_pInitialXboxDepthStencil, g_pInitialHostDepthStencil);
				g_pInitialHostDepthStencil->Release();
			}
		}

		g_pActiveXboxDepthStencil = g_pInitialXboxDepthStencil;

		UpdateDepthStencilFlags(g_pActiveXboxDepthStencil);
	}
#endif

	hRet = g_pD3DDevice8->CreateVertexBuffer
	(
		1, 0, 0, XTL::D3DPOOL_MANAGED,
		&g_pDummyBuffer
	);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexBuffer");

	for (int Streams = 0; Streams < MAX_NBR_STREAMS; Streams++)
	{
		// Dxbx note : Why do we need a dummy stream at all?
		hRet = g_pD3DDevice8->SetStreamSource(Streams, g_pDummyBuffer, 1);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetStreamSource");
	}

}

void CxbxClearD3DDeviceState()
{
	if (pClosingLineLoopIndexBuffer != nullptr) {
		pClosingLineLoopIndexBuffer->Release(); // TODO : This did crash - why?
		pClosingLineLoopIndexBuffer = nullptr;
		// Will be recreated by CxbxDrawIndexedClosingLine()
	}

	if (pQuadToTriangleD3DIndexBuffer != nullptr) {
		pQuadToTriangleD3DIndexBuffer->Release(); // TODO : This did crash - why?
		pQuadToTriangleD3DIndexBuffer = nullptr;
		// Will be recreated by CxbxAssureQuadListD3DIndexBuffer()
	}

	if (g_pDummyBuffer != nullptr) {
		g_pDummyBuffer->Release();
		g_pDummyBuffer = nullptr;
	}

	g_TextureCache.Clear(); // Make sure all textures will be reconverted
	// TODO : g_ConvertedIndexBuffers.Clear();
	// TODO : g_VertexBufferCache.Clear(); // See g_PatchedStreamsCache
	// TODO : g_VertexShaderCache.Clear();
	// TODO : g_PixelShaderCache.Clear();
}

void CxbxClearGlobals()
{
	// g_hEmuWindow = nullptr;
	g_pD3DDevice8 = nullptr;
	g_pDDSPrimary7 = nullptr;
	g_pDDSOverlay7 = nullptr;
	g_pDDClipper = nullptr;
	g_CurrentVertexShader = 0;
	//g_dwCurrentPixelShader = 0;
	//g_CurrentPixelShader = nullptr;
	//g_bFakePixelShaderLoaded = FALSE;
	g_bIsFauxFullscreen = FALSE;
	g_bHackUpdateSoftwareOverlay = FALSE;

	g_hMonitor = NULL;
	g_bSupportsYUY2Overlay = FALSE;
	g_pDD7 = nullptr;
	g_DriverCaps = { 0 };

	// KEEP g_XbeHeader = NULL;
	// KEEP g_XbeHeaderSize = 0;
	// KEEP g_hBgBrush = NULL;
	// KEEP g_bRenderWindowActive = false;
	// KEEP g_XBVideo = {};
	g_pVBCallback = NULL;
	g_pSwapCallback = NULL;
	g_pCallback = NULL;
	// g_CallbackType;
	// g_CallbackParam;
	g_bHasDepthBits = FALSE;
	g_bHasStencilBits = FALSE;
	// g_dwPrimPerFrame = 0;

	// KEEP g_ddguid;
	// KEEP g_pD3D8 = NULL;
	// KEEP g_D3DCaps = {};

	g_FillModeOverride = 0;
	X_D3DSCM_CORRECTION_VersionDependent = 0;

#if 0
	g_pIndexBuffer = nullptr;
	g_dwBaseVertexIndex = 0;
#endif

	g_VBData = { 0 };
	g_VBLastSwap = 0;

	g_SwapData = { 0 };
#if 0
	g_SwapLast = 0;
#endif

#if 0
	g_BackMaterial = { 0 };
#endif
#ifndef USE_XBOX_BUFFERS
	g_pInitialXboxBackBuffer = NULL;
	g_pInitialHostBackBuffer = nullptr;
	g_pActiveXboxBackBuffer = NULL;
#endif
	g_pActiveHostBackBuffer = nullptr;

#ifndef USE_XBOX_BUFFERS
	g_pInitialXboxRenderTarget = NULL;
	g_pInitialHostRenderTarget = nullptr;
	g_pActiveXboxRenderTarget = NULL;
	g_pActiveHostRenderTarget = nullptr;

	g_pInitialXboxDepthStencil = NULL;
	g_pInitialHostDepthStencil = nullptr;
	g_pActiveXboxDepthStencil = NULL;
	g_pActiveHostDepthStencil = nullptr;
#endif

#if 0
	g_pCachedYuvSurface = NULL;
#endif
	g_dwVertexShaderUsage = 0;
	memset(g_VertexShaderSlots, 0, D3DVS_XBOX_NR_ADDRESS_SLOTS * sizeof(DWORD)); // TODO : Use ARRAY_SIZE() and/or countof()
	// g_pTexturePaletteStages = { nullptr, nullptr, nullptr, nullptr };
	g_VertexShaderConstantMode = X_D3DSCM_192CONSTANTS;
	//XTL::EmuD3DTileCache = { 0 };
	// KEEP Xbox_D3DDevice_m_PixelShader
	// KEEP Xbox_D3DDevice_m_pTextures
	// KEEP Xbox_D3DDevice_m_pPalettes
	// KEEP Xbox_D3DDevice_m_pRenderTarget
	// KEEP Xbox_D3DDevice_m_pDepthStencil
	// KEEP g_EmuCDPD = { 0 };
}

// information passed to the create device proxy thread
struct EmuD3D8CreateDeviceProxyData
{
	XTL::  D3DDEVICE_CREATION_PARAMETERS HostCreationParameters; // Original host version
	XTL::X_D3DDEVICE_CREATION_PARAMETERS XboxCreationParameters; // Converted to Xbox
	XTL::X_D3DPRESENT_PARAMETERS         XboxPresentationParameters; // Original Xbox version
	XTL::  D3DPRESENT_PARAMETERS		 HostPresentationParameters; // Converted to Windows
//	XTL::IDirect3DDevice8              **ppReturnedDeviceInterface;
	volatile bool                        bSignalled;
    union
    {
        volatile HRESULT  hRet;
        volatile bool     bCreate;   // false : release
    };
}
g_EmuCDPD = {0};

const char *CxbxGetErrorDescription(HRESULT hResult)
{
	// TODO : For D3D9, Use DXGetErrorDescription9(hResult) (requires another DLL though)
	// See : http://www.fairyengine.com/articles/dxmultiviews.htm
	// and : http://www.gamedev.net/community/forums/showfaq.asp?forum_id=10
	// and : http://www.gamedev.net/community/forums/topic.asp?topic_id=16157
	switch (hResult)
	{
	case D3DERR_INVALIDCALL: return "Invalid Call";
	case D3DERR_NOTAVAILABLE: return "Not Available";
	// case D3DERR_OUTOFVIDEOMEMORY: return "Out of Video Memory"; // duplicate of DDERR_OUTOFVIDEOMEMORY

	case D3D_OK: return "No error occurred.";
#if 0
	case D3DERR_BADMAJORVERSION: return "The service that you requested is unavailable in this major version of DirectX. (A major version denotes a primary release, such as DirectX 6.0.) ";
	case D3DERR_BADMINORVERSION: return "The service that you requested is available in this major version of DirectX, but not in this minor version. Get the latest version of the component run time from Microsoft. (A minor version denotes a secondary release, such as DirectX 6.1.) ";
	case D3DERR_COLORKEYATTACHED: return "The application attempted to create a texture with a surface that uses a color key for transparency. ";
#endif
	case D3DERR_CONFLICTINGTEXTUREFILTER: return "The current texture filters cannot be used together. ";
	case D3DERR_CONFLICTINGTEXTUREPALETTE: return "The current textures cannot be used simultaneously. This generally occurs when a multitexture device requires that all palettized textures simultaneously enabled also share the same palette. ";
	case D3DERR_CONFLICTINGRENDERSTATE: return "The currently set render states cannot be used together. ";
#if 0
	case D3DERR_DEVICEAGGREGATED: return "The IDirect3DDevice7::SetRenderTarget method was called on a device that was retrieved from the render target surface. ";
	case D3DERR_EXECUTE_CLIPPED_FAILED: return "The execute buffer could not be clipped during execution. ";
	case D3DERR_EXECUTE_CREATE_FAILED: return "The execute buffer could not be created. This typically occurs when no memory is available to allocate the execute buffer. ";
	case D3DERR_EXECUTE_DESTROY_FAILED: return "The memory for the execute buffer could not be deallocated. ";
	case D3DERR_EXECUTE_FAILED: return "The contents of the execute buffer are invalid and cannot be executed. ";
	case D3DERR_EXECUTE_LOCK_FAILED: return "The execute buffer could not be locked. ";
	case D3DERR_EXECUTE_LOCKED: return "The operation requested by the application could not be completed because the execute buffer is locked. ";
	case D3DERR_EXECUTE_NOT_LOCKED: return "The execute buffer could not be unlocked because it is not currently locked. ";
	case D3DERR_EXECUTE_UNLOCK_FAILED: return "The execute buffer could not be unlocked. ";
	case D3DERR_INBEGIN: return "The requested operation cannot be completed while scene rendering is taking place. Try again after the scene is completed and the IDirect3DDevice7::EndScene method is called. ";
	case D3DERR_INBEGINSTATEBLOCK: return "The operation cannot be completed while recording states for a state block. Complete recording by calling the IDirect3DDevice7::EndStateBlock method, and try again. ";
	case D3DERR_INITFAILED: return "A rendering device could not be created because the new device could not be initialized. ";
	case D3DERR_INVALID_DEVICE: return "The requested device type is not valid. ";
	case D3DERR_INVALIDCURRENTVIEWPORT: return "The currently selected viewport is not valid. ";
	case D3DERR_INVALIDMATRIX: return "The requested operation could not be completed because the combination of the currently set world, view, and projection matrices is invalid (the determinant of the combined matrix is 0). ";
	case D3DERR_INVALIDPALETTE: return "The palette associated with a surface is invalid. ";
	case D3DERR_INVALIDPRIMITIVETYPE: return "The primitive type specified by the application is invalid. ";
	case D3DERR_INVALIDRAMPTEXTURE: return "Ramp mode is being used, and the texture handle in the current material does not match the current texture handle that is set as a render state. ";
	case D3DERR_INVALIDSTATEBLOCK: return "The state block handle is invalid. ";
	case D3DERR_INVALIDVERTEXFORMAT: return "The combination of flexible vertex format flags specified by the application is not valid. ";
	case D3DERR_INVALIDVERTEXTYPE: return "The vertex type specified by the application is invalid. ";
	case D3DERR_LIGHT_SET_FAILED: return "The attempt to set lighting parameters for a light object failed. ";
	case D3DERR_LIGHTHASVIEWPORT: return "The requested operation failed because the light object is associated with another viewport. ";
	case D3DERR_LIGHTNOTINTHISVIEWPORT: return "The requested operation failed because the light object has not been associated with this viewport. ";
	case D3DERR_MATERIAL_CREATE_FAILED: return "The material could not be created. This typically occurs when no memory is available to allocate for the material. ";
	case D3DERR_MATERIAL_DESTROY_FAILED: return "The memory for the material could not be deallocated. ";
	case D3DERR_MATERIAL_GETDATA_FAILED: return "The material parameters could not be retrieved. ";
	case D3DERR_MATERIAL_SETDATA_FAILED: return "The material parameters could not be set. ";
	case D3DERR_MATRIX_CREATE_FAILED: return "The matrix could not be created. This can occur when no memory is available to allocate for the matrix. ";
	case D3DERR_MATRIX_DESTROY_FAILED: return "The memory for the matrix could not be deallocated. ";
	case D3DERR_MATRIX_GETDATA_FAILED: return "The matrix data could not be retrieved. This can occur when the matrix was not created by the current device. ";
	case D3DERR_MATRIX_SETDATA_FAILED: return "The matrix data could not be set. This can occur when the matrix was not created by the current device. ";
	case D3DERR_NOCURRENTVIEWPORT: return "The viewport parameters could not be retrieved because none have been set. ";
	case D3DERR_NOTINBEGIN: return "The requested rendering operation could not be completed because scene rendering has not begun. Call IDirect3DDevice7::BeginScene to begin rendering, and try again. ";
	case D3DERR_NOTINBEGINSTATEBLOCK: return "The requested operation could not be completed because it is only valid while recording a state block. Call the IDirect3DDevice7::BeginStateBlock method, and try again. ";
	case D3DERR_NOVIEWPORTS: return "The requested operation failed because the device currently has no viewports associated with it. ";
	case D3DERR_SCENE_BEGIN_FAILED: return "Scene rendering could not begin. ";
	case D3DERR_SCENE_END_FAILED: return "Scene rendering could not be completed. ";
	case D3DERR_SCENE_IN_SCENE: return "Scene rendering could not begin because a previous scene was not completed by a call to the IDirect3DDevice7::EndScene method. ";
	case D3DERR_SCENE_NOT_IN_SCENE: return "Scene rendering could not be completed because a scene was not started by a previous call to the IDirect3DDevice7::BeginScene method. ";
	case D3DERR_SETVIEWPORTDATA_FAILED: return "The viewport parameters could not be set. ";
	case D3DERR_STENCILBUFFER_NOTPRESENT: return "The requested stencil buffer operation could not be completed because there is no stencil buffer attached to the render target surface. ";
	case D3DERR_SURFACENOTINVIDMEM: return "The device could not be created because the render target surface is not located in video memory. (Hardware-accelerated devices require video-memory render target surfaces.) ";
	case D3DERR_TEXTURE_BADSIZE: return "The dimensions of a current texture are invalid. This can occur when an application attempts to use a texture that has dimensions that are not a power of 2 with a device that requires them. ";
	case D3DERR_TEXTURE_CREATE_FAILED: return "The texture handle for the texture could not be retrieved from the driver. ";
	case D3DERR_TEXTURE_DESTROY_FAILED: return "The device was unable to deallocate the texture memory. ";
	case D3DERR_TEXTURE_GETSURF_FAILED: return "The DirectDraw surface used to create the texture could not be retrieved. ";
	case D3DERR_TEXTURE_LOAD_FAILED: return "The texture could not be loaded. ";
	case D3DERR_TEXTURE_LOCK_FAILED: return "The texture could not be locked. ";
	case D3DERR_TEXTURE_LOCKED: return "The requested operation could not be completed because the texture surface is currently locked. ";
	case D3DERR_TEXTURE_NO_SUPPORT: return "The device does not support texture mapping. ";
	case D3DERR_TEXTURE_NOT_LOCKED: return "The requested operation could not be completed because the texture surface is not locked. ";
	case D3DERR_TEXTURE_SWAP_FAILED: return "The texture handles could not be swapped. ";
	case D3DERR_TEXTURE_UNLOCK_FAILED: return "The texture surface could not be unlocked. ";
#endif
	case D3DERR_TOOMANYOPERATIONS: return "The application is requesting more texture-filtering operations than the device supports. ";
#if 0
	case D3DERR_TOOMANYPRIMITIVES: return "The device is unable to render the provided number of primitives in a single pass. ";
#endif
	case D3DERR_UNSUPPORTEDALPHAARG: return "The device does not support one of the specified texture-blending arguments for the alpha channel. ";
	case D3DERR_UNSUPPORTEDALPHAOPERATION: return "The device does not support one of the specified texture-blending operations for the alpha channel. ";
	case D3DERR_UNSUPPORTEDCOLORARG: return "The device does not support one of the specified texture-blending arguments for color values. ";
	case D3DERR_UNSUPPORTEDCOLOROPERATION: return "The device does not support one of the specified texture-blending operations for color values. ";
	case D3DERR_UNSUPPORTEDFACTORVALUE: return "The specified texture factor value is not supported by the device. ";
	case D3DERR_UNSUPPORTEDTEXTUREFILTER: return "The specified texture filter is not supported by the device. ";
#if 0
	case D3DERR_VBUF_CREATE_FAILED: return "The vertex buffer could not be created. This can happen when there is insufficient memory to allocate a vertex buffer. ";
	case D3DERR_VERTEXBUFFERLOCKED: return "The requested operation could not be completed because the vertex buffer is locked. ";
	case D3DERR_VERTEXBUFFEROPTIMIZED: return "The requested operation could not be completed because the vertex buffer is optimized. (The contents of optimized vertex buffers are driver-specific and considered private.) ";
	case D3DERR_VERTEXBUFFERUNLOCKFAILED: return "The vertex buffer could not be unlocked because the vertex buffer memory was overrun. Be sure that your application does not write beyond the size of the vertex buffer. ";
	case D3DERR_VIEWPORTDATANOTSET: return "The requested operation could not be completed because viewport parameters have not yet been set. Set the viewport parameters by calling the IDirect3DDevice7::SetViewport method, and try again. ";
	case D3DERR_VIEWPORTHASNODEVICE: return "The requested operation could not be completed because the viewport has not yet been associated with a device. Associate the viewport with a rendering device by calling the IDirect3DDevice3::AddViewport method, and try again. ";
#endif
	case D3DERR_WRONGTEXTUREFORMAT: return "The pixel format of the texture surface is not valid. ";
#if 0
	case D3DERR_ZBUFF_NEEDS_SYSTEMMEMORY: return "The requested operation could not be completed because the specified device requires system-memory depth-buffer surfaces. (Software rendering devices require system-memory depth buffers.) ";
	case D3DERR_ZBUFF_NEEDS_VIDEOMEMORY: return "The requested operation could not be completed because the specified device requires video-memory depth-buffer surfaces. (Hardware-accelerated devices require video-memory depth buffers.) ";
	case D3DERR_ZBUFFER_NOTPRESENT: return "The requested operation could not be completed because the render target surface does not have an attached depth buffer. ";
	case DD_OK: return "The request completed successfully.";
#endif
	case DDERR_ALREADYINITIALIZED: return "The object has already been initialized.";
	case DDERR_BLTFASTCANTCLIP: return "A DirectDrawClipper object is attached to a source surface that has passed into a call to the IDirectDrawSurface7::BltFast method.";
	case DDERR_CANNOTATTACHSURFACE: return "A surface cannot be attached to another requested surface.";
	case DDERR_CANNOTDETACHSURFACE: return "A surface cannot be detached from another requested surface.";
	case DDERR_CANTCREATEDC: return "Windows cannot create any more device contexts (DCs), or a DC has requested a palette-indexed surface when the surface had no palette and the display mode was not palette-indexed (in this case DirectDraw cannot select a proper palette into the DC).";
	case DDERR_CANTDUPLICATE: return "Primary and 3-D surfaces, or surfaces that are implicitly created, cannot be duplicated.";
	case DDERR_CANTLOCKSURFACE: return "Access to this surface is refused because an attempt was made to lock the primary surface without DCI support.";
	case DDERR_CANTPAGELOCK: return "An attempt to page-lock a surface failed. Page lock does not work on a display-memory surface or an emulated primary surface.";
	case DDERR_CANTPAGEUNLOCK: return "An attempt to page-unlock a surface failed. Page unlock does not work on a display-memory surface or an emulated primary surface.";
	case DDERR_CLIPPERISUSINGHWND: return "An attempt was made to set a clip list for a DirectDrawClipper object that is already monitoring a window handle.";
	case DDERR_COLORKEYNOTSET: return "No source color key is specified for this operation.";
	case DDERR_CURRENTLYNOTAVAIL: return "No support is currently available.";
	case DDERR_DDSCAPSCOMPLEXREQUIRED: return "New for DirectX 7.0. The surface requires the DDSCAPS_COMPLEX flag.";
	case DDERR_DCALREADYCREATED: return "A device context (DC) has already been returned for this surface. Only one DC can be retrieved for each surface.";
	case DDERR_DEVICEDOESNTOWNSURFACE: return "Surfaces created by one DirectDraw device cannot be used directly by another DirectDraw device.";
	case DDERR_DIRECTDRAWALREADYCREATED: return "A DirectDraw object representing this driver has already been created for this process.";
	case DDERR_EXCEPTION: return "An exception was encountered while performing the requested operation.";
	case DDERR_EXCLUSIVEMODEALREADYSET: return "An attempt was made to set the cooperative level when it was already set to exclusive.";
	case DDERR_EXPIRED: return "The data has expired and is therefore no longer valid.";
	case DDERR_GENERIC: return "There is an undefined error condition.";
	case DDERR_HEIGHTALIGN: return "The height of the provided rectangle is not a multiple of the required alignment.";
	case DDERR_HWNDALREADYSET: return "The DirectDraw cooperative-level window handle has already been set. It cannot be reset while the process has surfaces or palettes created.";
	case DDERR_HWNDSUBCLASSED: return "DirectDraw is prevented from restoring state because the DirectDraw cooperative-level window handle has been subclassed.";
	case DDERR_IMPLICITLYCREATED: return "The surface cannot be restored because it is an implicitly created surface.";
	case DDERR_INCOMPATIBLEPRIMARY: return "The primary surface creation request does not match the existing primary surface.";
	case DDERR_INVALIDCAPS: return "One or more of the capability bits passed to the callback function are incorrect.";
	case DDERR_INVALIDCLIPLIST: return "DirectDraw does not support the provided clip list.";
	case DDERR_INVALIDDIRECTDRAWGUID: return "The globally unique identifier (GUID) passed to the DirectDrawCreate function is not a valid DirectDraw driver identifier.";
	case DDERR_INVALIDMODE: return "DirectDraw does not support the requested mode.";
	case DDERR_INVALIDOBJECT: return "DirectDraw received a pointer that was an invalid DirectDraw object.";
	case DDERR_INVALIDPARAMS: return "One or more of the parameters passed to the method are incorrect.";
	case DDERR_INVALIDPIXELFORMAT: return "The pixel format was invalid as specified.";
	case DDERR_INVALIDPOSITION: return "The position of the overlay on the destination is no longer legal.";
	case DDERR_INVALIDRECT: return "The provided rectangle was invalid.";
	case DDERR_INVALIDSTREAM: return "The specified stream contains invalid data.";
	case DDERR_INVALIDSURFACETYPE: return "The surface was of the wrong type.";
	case DDERR_LOCKEDSURFACES: return "One or more surfaces are locked, causing the failure of the requested operation.";
	case DDERR_MOREDATA: return "There is more data available than the specified buffer size can hold.";
	case DDERR_NEWMODE: return "New for DirectX 7.0. When IDirectDraw7::StartModeTest is called with the DDSMT_ISTESTREQUIRED flag, it may return this value to denote that some or all of the resolutions can and should be tested. IDirectDraw7::EvaluateMode returns this value to indicate that the test has switched to a new display mode.";
	case DDERR_NO3D: return "No 3-D hardware or emulation is present.";
	case DDERR_NOALPHAHW: return "No alpha-acceleration hardware is present or available, causing the failure of the requested operation.";
	case DDERR_NOBLTHW: return "No blitter hardware is present.";
	case DDERR_NOCLIPLIST: return "No clip list is available.";
	case DDERR_NOCLIPPERATTACHED: return "No DirectDrawClipper object is attached to the surface object.";
	case DDERR_NOCOLORCONVHW: return "No color-conversion hardware is present or available.";
	case DDERR_NOCOLORKEY: return "The surface does not currently have a color key.";
	case DDERR_NOCOLORKEYHW: return "There is no hardware support for the destination color key.";
	case DDERR_NOCOOPERATIVELEVELSET: return "A create function was called without the IDirectDraw7::SetCooperativeLevel method.";
	case DDERR_NODC: return "No device context (DC) has ever been created for this surface.";
	case DDERR_NODDROPSHW: return "No DirectDraw raster-operation (ROP) hardware is available.";
	case DDERR_NODIRECTDRAWHW: return "Hardware-only DirectDraw object creation is not possible; the driver does not support any hardware.";
	case DDERR_NODIRECTDRAWSUPPORT: return "DirectDraw support is not possible with the current display driver.";
	case DDERR_NODRIVERSUPPORT: return "New for DirectX 7.0. Testing cannot proceed because the display adapter driver does not enumerate refresh rates.";
	case DDERR_NOEMULATION: return "Software emulation is not available.";
	case DDERR_NOEXCLUSIVEMODE: return "The operation requires the application to have exclusive mode, but the application does not have exclusive mode.";
	case DDERR_NOFLIPHW: return "Flipping visible surfaces is not supported.";
	case DDERR_NOFOCUSWINDOW: return "An attempt was made to create or set a device window without first setting the focus window.";
	case DDERR_NOGDI: return "No GDI is present.";
	case DDERR_NOHWND: return "Clipper notification requires a window handle, or no window handle has been previously set as the cooperative level window handle.";
	case DDERR_NOMIPMAPHW: return "No mipmap-capable texture mapping hardware is present or available.";
	case DDERR_NOMIRRORHW: return "No mirroring hardware is present or available.";
	case DDERR_NOMONITORINFORMATION: return "New for DirectX 7.0. Testing cannot proceed because the monitor has no associated EDID data.";
	case DDERR_NONONLOCALVIDMEM: return "An attempt was made to allocate nonlocal video memory from a device that does not support nonlocal video memory.";
	case DDERR_NOOPTIMIZEHW: return "The device does not support optimized surfaces.";
	case DDERR_NOOVERLAYDEST: return "The IDirectDrawSurface7::GetOverlayPosition method is called on an overlay that the IDirectDrawSurface7::UpdateOverlay method has not been called on to establish as a destination.";
	case DDERR_NOOVERLAYHW: return "No overlay hardware is present or available.";
	case DDERR_NOPALETTEATTACHED: return "No palette object is attached to this surface.";
	case DDERR_NOPALETTEHW: return "There is no hardware support for 16- or 256-color palettes.";
	case DDERR_NORASTEROPHW: return "No appropriate raster-operation hardware is present or available.";
	case DDERR_NOROTATIONHW: return "No rotation hardware is present or available.";
	case DDERR_NOSTEREOHARDWARE: return "There is no stereo hardware present or available.";
	case DDERR_NOSTRETCHHW: return "There is no hardware support for stretching.";
	case DDERR_NOSURFACELEFT: return "There is no hardware present that supports stereo surfaces.";
	case DDERR_NOT4BITCOLOR: return "The DirectDrawSurface object is not using a 4-bit color palette, and the requested operation requires a 4-bit color palette.";
	case DDERR_NOT4BITCOLORINDEX: return "The DirectDrawSurface object is not using a 4-bit color index palette, and the requested operation requires a 4-bit color index palette.";
	case DDERR_NOT8BITCOLOR: return "The DirectDrawSurface object is not using an 8-bit color palette, and the requested operation requires an 8-bit color palette.";
	case DDERR_NOTAOVERLAYSURFACE: return "An overlay component is called for a nonoverlay surface.";
	case DDERR_NOTEXTUREHW: return "The operation cannot be carried out because no texture-mapping hardware is present or available.";
	case DDERR_NOTFLIPPABLE: return "An attempt was made to flip a surface that cannot be flipped.";
	case DDERR_NOTFOUND: return "The requested item was not found.";
	case DDERR_NOTINITIALIZED: return "An attempt was made to call an interface method of a DirectDraw object created by CoCreateInstance before the object was initialized.";
	case DDERR_NOTLOADED: return "The surface is an optimized surface, but it has not yet been allocated any memory.";
	case DDERR_NOTLOCKED: return "An attempt was made to unlock a surface that was not locked.";
	case DDERR_NOTPAGELOCKED: return "An attempt was made to page-unlock a surface with no outstanding page locks.";
	case DDERR_NOTPALETTIZED: return "The surface being used is not a palette-based surface.";
	case DDERR_NOVSYNCHW: return "There is no hardware support for vertical blanksynchronized operations.";
	case DDERR_NOZBUFFERHW: return "The operation to create a z-buffer in display memory or to perform a blit, using a z-buffer cannot be carried out because there is no hardware support for z-buffers.";
	case DDERR_NOZOVERLAYHW: return "The overlay surfaces cannot be z-layered, based on the z-order because the hardware does not support z-ordering of overlays.";
	case DDERR_OUTOFCAPS: return "The hardware needed for the requested operation has already been allocated.";
	case DDERR_OUTOFMEMORY: return "DirectDraw does not have enough memory to perform the operation.";
	case DDERR_OUTOFVIDEOMEMORY: return "DirectDraw does not have enough display memory to perform the operation.";
	case DDERR_OVERLAPPINGRECTS: return "The source and destination rectangles are on the same surface and overlap each other.";
	case DDERR_OVERLAYCANTCLIP: return "The hardware does not support clipped overlays.";
	case DDERR_OVERLAYCOLORKEYONLYONEACTIVE: return "An attempt was made to have more than one color key active on an overlay.";
	case DDERR_OVERLAYNOTVISIBLE: return "The IDirectDrawSurface7::GetOverlayPosition method was called on a hidden overlay.";
	case DDERR_PALETTEBUSY: return "Access to this palette is refused because the palette is locked by another thread.";
	case DDERR_PRIMARYSURFACEALREADYEXISTS: return "This process has already created a primary surface.";
	case DDERR_REGIONTOOSMALL: return "The region passed to the IDirectDrawClipper::GetClipList method is too small.";
	case DDERR_SURFACEALREADYATTACHED: return "An attempt was made to attach a surface to another surface to which it is already attached.";
	case DDERR_SURFACEALREADYDEPENDENT: return "An attempt was made to make a surface a dependency of another surface on which it is already dependent.";
	case DDERR_SURFACEBUSY: return "Access to the surface is refused because the surface is locked by another thread.";
	case DDERR_SURFACEISOBSCURED: return "Access to the surface is refused because the surface is obscured.";
	case DDERR_SURFACELOST: return "Access to the surface is refused because the surface memory is gone. Call the IDirectDrawSurface7::Restore method on this surface to restore the memory associated with it.";
	case DDERR_SURFACENOTATTACHED: return "The requested surface is not attached.";
	case DDERR_TESTFINISHED: return "New for DirectX 7.0. When returned by the IDirectDraw7::StartModeTest method, this value means that no test could be initiated because all the resolutions chosen for testing already have refresh rate information in the registry. When returned by IDirectDraw7::EvaluateMode, the value means that DirectDraw has completed a refresh rate test.";
	case DDERR_TOOBIGHEIGHT: return "The height requested by DirectDraw is too large.";
	case DDERR_TOOBIGSIZE: return "The size requested by DirectDraw is too large. However, the individual height and width are valid sizes.";
	case DDERR_TOOBIGWIDTH: return "The width requested by DirectDraw is too large.";
	case DDERR_UNSUPPORTED: return "The operation is not supported.";
	case DDERR_UNSUPPORTEDFORMAT: return "The pixel format requested is not supported by DirectDraw.";
	case DDERR_UNSUPPORTEDMASK: return "The bitmask in the pixel format requested is not supported by DirectDraw.";
	case DDERR_UNSUPPORTEDMODE: return "The display is currently in an unsupported mode.";
	case DDERR_VERTICALBLANKINPROGRESS: return "A vertical blank is in progress.";
	case DDERR_VIDEONOTACTIVE: return "The video port is not active.";
	case DDERR_WASSTILLDRAWING: return "The previous blit operation that is transferring information to or from this surface is incomplete.";
	case DDERR_WRONGMODE: return "This surface cannot be restored because it was created in a different mode.";
	case DDERR_XALIGN: return "The provided rectangle was not horizontally aligned on a required boundary.";
	}

	return nullptr;
}

const char *D3DErrorString(HRESULT hResult)
{
	static char buffer[1024];
	buffer[0] = 0; // Reset static buffer!

	const char* errorCodeString = XTL::DXGetErrorString8A(hResult);
	if (errorCodeString)
	{
		strcat(buffer, errorCodeString);
		strcat(buffer, ": ");
	}

	const char* errorDescription = CxbxGetErrorDescription(hResult);
	if (errorDescription)
		strcat(buffer, errorDescription);
	else
		strcat(buffer, "Unknown D3D error.");

	return buffer;
}

VOID XTL::CxbxInitWindow(Xbe::Header *XbeHeader, uint32 XbeHeaderSize)
{
    g_EmuShared->GetXBVideo(&g_XBVideo);

    if(g_XBVideo.GetFullscreen())
        CxbxKrnl_hEmuParent = NULL;

    // cache XbeHeader and size of XbeHeader
    g_XbeHeader     = XbeHeader;
    g_XbeHeaderSize = XbeHeaderSize;

    // create timing thread
    {
        DWORD dwThreadId = 0;
        HANDLE hTimingThread = CreateThread(nullptr, 0, EmuUpdateTickCount, nullptr, 0, &dwThreadId);

		DbgPrintf("INIT: Created timing thread. Handle : 0x%X, ThreadId : [0x%.4X]\n", hTimingThread, dwThreadId);
        // We set the priority of this thread a bit higher, to assure reliable timing :
        SetThreadPriority(hTimingThread, THREAD_PRIORITY_ABOVE_NORMAL);

        // we must duplicate this handle in order to retain Suspend/Resume thread rights from a remote thread
        {
            HANDLE hDupHandle = NULL;

            DuplicateHandle(g_CurrentProcessHandle, hTimingThread, g_CurrentProcessHandle, &hDupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);

            CxbxKrnlRegisterThread(hDupHandle);
        }
    }

/* TODO : Port this Dxbx code :
  // create vblank handling thread
    {
        DWORD dwThreadId = 0;
        HANDLE hVBlankThread = CreateThread(nullptr, 0, EmuThreadHandleVBlank, nullptr, 0, &dwThreadId);
		DbgPrintf("INIT: Created VBlank thread. Handle : 0x%X, ThreadId : [0x%.4X]\n", hVBlankThread, dwThreadId);
    }
*/
    // create window message processing thread
    {
        DWORD dwThreadId = 0;

        g_bRenderWindowActive = false;

        HANDLE hRenderWindowThread = CreateThread(nullptr, 0, EmuRenderWindow, nullptr, 0, &dwThreadId);

		if (hRenderWindowThread == NULL) {
			CxbxPopupMessage("Creating EmuRenderWindowThread Failed: %08X", GetLastError());
			EmuShared::Cleanup();
			ExitProcess(0);
		}

		DbgPrintf("INIT: Created Message-Pump thread. Handle : 0x%X, ThreadId : [0x%.4X]\n", hRenderWindowThread, dwThreadId);

		// Ported from Dxbx :
		// If possible, assign this thread to another core than the one that runs Xbox1 code :
		SetThreadAffinityMask(hRenderWindowThread, g_CPUOthers);

        while(!g_bRenderWindowActive)
            SwitchToThread(); 

		SwitchToThread();
    }

	SetFocus(g_hEmuWindow);
}

struct Decoded_D3DCommon {
	uint uiRefCount;
	uint uiType;
	uint uiIntRefCount;
	bool bD3DCreated;
	//bool bIsLocked;
	//uint uiUnused;
};

void DecodeD3DCommon(DWORD D3DCommon, Decoded_D3DCommon &decoded)
{
	decoded.uiRefCount = D3DCommon & X_D3DCOMMON_REFCOUNT_MASK;
	decoded.uiType = D3DCommon & X_D3DCOMMON_TYPE_MASK;
	decoded.uiIntRefCount = (D3DCommon & X_D3DCOMMON_INTREFCOUNT_MASK) >> X_D3DCOMMON_INTREFCOUNT_SHIFT;
	decoded.bD3DCreated = (D3DCommon & X_D3DCOMMON_D3DCREATED) > 0;
	//decoded.bIsLocked = D3DCommon & X_D3DCOMMON_ISLOCKED) == X_D3DCOMMON_ISLOCKED;
	//decoded.uiUnused = (D3DCommon & X_D3DCOMMON_UNUSED_MASK) >> X_D3DCOMMON_UNUSED_SHIFT;
}

struct Decoded_D3DFormat {
	uint uiDMAChannel; // 1 = DMA channel A - the default for all system memory, 2 = DMA channel B - unused
	bool bIsCubeMap; // Set if the texture if a cube map
	bool bIsBorderTexture; // Set if the texture has a 4 texel wide additional border
	uint uiDimensions; // # of dimensions, must be 2 or 3
	XTL::X_D3DFORMAT X_Format; // D3DFORMAT - See X_D3DFMT_* 
	uint uiMipMapLevels; // 1-10 mipmap levels (always 1 for surfaces)
	uint uiUSize; // Log2 of the U size of the base texture (only set for swizzled or compressed)
	uint uiVSize; // Log2 of the V size of the base texture (only set for swizzled or compressed)
	uint uiPSize; // Log2 of the P size of the base texture (only set for swizzled or compressed)
};

void DumpDecodedD3DFormat(Decoded_D3DFormat &decoded)
{
	DbgPrintf("uiDMAChannel = %u\n", decoded.uiDMAChannel);
	DbgPrintf("bIsCubeMap = %d\n", decoded.bIsCubeMap);
	DbgPrintf("bIsBorderTexture = %d\n", decoded.bIsBorderTexture);
	DbgPrintf("uiDimensions = %u\n", decoded.uiDimensions);
	DbgPrintf("X_Format = 0x%.02X (%s)\n", decoded.X_Format, TYPE2PCHAR(X_D3DFORMAT)(decoded.X_Format));
	DbgPrintf("uiMipMapLevels = %u\n", decoded.uiMipMapLevels);
	DbgPrintf("uiUSize = %u\n", decoded.uiUSize);
	DbgPrintf("uiVSize = %u\n", decoded.uiVSize);
	DbgPrintf("uiPSize = %u\n", decoded.uiPSize);
}

void DecodeD3DFormat(DWORD D3DFormat, Decoded_D3DFormat &decoded)
{
	decoded.uiDMAChannel = D3DFormat & X_D3DFORMAT_DMACHANNEL_MASK;
	// Cubemap textures have the X_D3DFORMAT_CUBEMAP bit set (also, their size is 6 times a normal texture)
	decoded.bIsCubeMap = (D3DFormat & X_D3DFORMAT_CUBEMAP) > 0;
	decoded.bIsBorderTexture = (D3DFormat & X_D3DFORMAT_BORDERSOURCE_COLOR) == 0;
	decoded.uiDimensions = (D3DFormat & X_D3DFORMAT_DIMENSION_MASK) >> X_D3DFORMAT_DIMENSION_SHIFT;
	decoded.X_Format = (XTL::X_D3DFORMAT)((D3DFormat & X_D3DFORMAT_FORMAT_MASK) >> X_D3DFORMAT_FORMAT_SHIFT);
	decoded.uiMipMapLevels = (D3DFormat & X_D3DFORMAT_MIPMAP_MASK) >> X_D3DFORMAT_MIPMAP_SHIFT;
	decoded.uiUSize = (D3DFormat & X_D3DFORMAT_USIZE_MASK) >> X_D3DFORMAT_USIZE_SHIFT;
	decoded.uiVSize = (D3DFormat & X_D3DFORMAT_VSIZE_MASK) >> X_D3DFORMAT_VSIZE_SHIFT;
	decoded.uiPSize = (D3DFormat & X_D3DFORMAT_PSIZE_MASK) >> X_D3DFORMAT_PSIZE_SHIFT;
}

struct Decoded_D3DSize {
	uint uiWidth; // Number of pixels on 1 line (measured at mipmap level 0)
	uint uiHeight; // Number of lines (measured at mipmap level 0)
	uint uiPitch; // Bytes to skip to get to the next line (measured at mipmap level 0)
};

void DumpDecodedD3DSize(Decoded_D3DSize &decoded)
{
	DbgPrintf("uiWidth = %u\n", decoded.uiWidth);
	DbgPrintf("uiHeight = %u\n", decoded.uiHeight);
	DbgPrintf("uiPitch = %u\n", decoded.uiPitch);
}

void DecodeD3DSize(DWORD D3DSize, Decoded_D3DSize &decoded)
{
	decoded.uiWidth = ((D3DSize & X_D3DSIZE_WIDTH_MASK) /* >> X_D3DSIZE_WIDTH_SHIFT*/) + 1;
	decoded.uiHeight = ((D3DSize & X_D3DSIZE_HEIGHT_MASK) >> X_D3DSIZE_HEIGHT_SHIFT) + 1;
	decoded.uiPitch = (((D3DSize & X_D3DSIZE_PITCH_MASK) >> X_D3DSIZE_PITCH_SHIFT) + 1) * X_D3DTEXTURE_PITCH_ALIGNMENT;
}

#define X_MAX_MIPMAPS_VOLUME 9 // 2^9 = 512, the maximum NV2A volume texture dimension size
#define X_MAX_MIPMAPS 12 // 2^12 = 4096, the maximum NV2A (2D) texture dimension size

struct DecodedPixelContainer {
	XTL::X_D3DPixelContainer *pPixelContainer;
	Decoded_D3DFormat format;
	DWORD dwBPP; // Bits per pixel, 8, 16 or 32 (and 4 for DXT1)
	BOOL bIsSwizzled;
	BOOL bIsCompressed;
	BOOL bIs3D;
	DWORD dwWidth; // Number of pixels on 1 line (measured at mipmap level 0)
	DWORD dwHeight; // Number of lines (measured at mipmap level 0)
	DWORD dwDepth; // Volume texture number of slices
	DWORD dwRowPitch; // Bytes to skip to get to the next line (measured at mipmap level 0)
	DWORD dwFacePitch; // Cube maps must step this number of bytes per face to skip (this includes all mipmaps for 1 face)
	uint SlicePitches[X_MAX_MIPMAPS]; // Size of a slice per mipmap level (also zero based)
	DWORD dwMipMapLevels; // 1-10 mipmap levels (always 1 for surfaces)
#ifdef INCLUDE_D3DFORMAT_PER_MIPMAP
	DWORD MipMapFormats[X_MAX_MIPMAPS]; // Size-adjusted D3DFormat per mipmap level
#endif
	uint MipMapOffsets[X_MAX_MIPMAPS]; // [0]=0, [1]=level 0 size (=level 1 offset=volume slice size), [2..10]=offsets of those levels
	uint MipMapSlices[X_MAX_MIPMAPS]; // Number of slices per mipmap level
	DWORD dwMinXYValue; // 4 for compressed formats, 1 for everything else. Only applies to width & height. Depth can go to 1.
};

void DumpDecodedPixelContainer(DecodedPixelContainer &decoded)
{
	DbgPrintf("pPixelContainer = 0x%.08X\n", decoded.pPixelContainer);
	DumpDecodedD3DFormat(decoded.format);
	DbgPrintf("dwBPP = %d\n", decoded.dwBPP);
	DbgPrintf("bIsSwizzled = %d\n", decoded.bIsSwizzled);
	DbgPrintf("bIsCompressed = %d\n", decoded.bIsCompressed);
	DbgPrintf("bIs3D = %d\n", decoded.bIs3D);
	DbgPrintf("dwWidth = %d\n", decoded.dwWidth);
	DbgPrintf("dwHeight = %d\n", decoded.dwHeight);
	DbgPrintf("dwDepth = %d\n", decoded.dwDepth);
	DbgPrintf("dwRowPitch = %d\n", decoded.dwRowPitch);
	DbgPrintf("dwFacePitch = %d\n", decoded.dwFacePitch);
	for (uint level = 0; level < decoded.dwMipMapLevels; level++)
		DbgPrintf("SlicePitches[%d] = %d\n", level, decoded.SlicePitches[level]);
#ifdef INCLUDE_D3DFORMAT_PER_MIPMAP
	for (uint level = 0; level < decoded.dwMipMapLevels; level++)
		DbgPrintf("MipMapFormats[%d] = %.8X\n", level, decoded.MipMapFormats[level]);
#endif
	for (uint level = 0; level < max(2, decoded.dwMipMapLevels); level++) // at least show up to [1]=level 0 size (=level 1 offset=volume slice size)
		DbgPrintf("MipMapOffsets[%d] = %d\n", level, decoded.MipMapOffsets[level]);
	for (uint level = 0; level < decoded.dwMipMapLevels; level++)
		DbgPrintf("MipMapSlices[%d] = %d\n", level, decoded.MipMapSlices[level]);
	DbgPrintf("dwMinXYValue = %d\n", decoded.dwMinXYValue);
}

void DecodeD3DFormatAndSize(XTL::X_D3DPixelContainer *pPixelContainer, OUT DecodedPixelContainer &decoded)
{
	decoded.pPixelContainer = pPixelContainer;

	DWORD dwD3DFormat = pPixelContainer->Format;
	DWORD dwD3DSize = pPixelContainer->Size;

	DecodeD3DFormat(dwD3DFormat, decoded.format);

	decoded.dwBPP = EmuXBFormatBitsPerPixel(decoded.format.X_Format);
	decoded.bIsSwizzled = EmuXBFormatIsSwizzled(decoded.format.X_Format);
	decoded.bIsCompressed = EmuXBFormatIsCompressed(decoded.format.X_Format);
	// if(decoded.format.bIsBorderTexture) // TODO : increase size (left,top,right,bottom) with 4 texel border ALSO for mipmaps?
	// Volumes have 3 dimensions, the rest have 2.
	decoded.bIs3D = (decoded.format.uiDimensions == 3);

	if (dwD3DSize != 0) {
		// This case cannot be reached for Cube maps or Volumes, as those use the 'power of two' "format" :
		Decoded_D3DSize decodedSize;
		DecodeD3DSize(dwD3DSize, decodedSize);

		decoded.dwWidth = decodedSize.uiWidth;
		decoded.dwHeight = decodedSize.uiHeight;
		decoded.dwDepth = 1;
		decoded.dwRowPitch = decodedSize.uiPitch;
		decoded.SlicePitches[0] = decodedSize.uiPitch * decoded.dwHeight;
		decoded.dwMipMapLevels = 1;
	}
	else {
		// This case uses 'power of two' format, which can be used by Cube maps and Volumes
		// (which means we have to calculate Face and Slice pitch too) :
		decoded.dwWidth = 1 << decoded.format.uiUSize;
		decoded.dwHeight = 1 << decoded.format.uiVSize;
		decoded.dwDepth = 1 << decoded.format.uiPSize;
		uint _RowBits = decoded.dwWidth * decoded.dwBPP;
		if (decoded.bIsCompressed)
			// Compressed formats encode 4x4 texels per block.
			// DXT1 has 4 BPP (must become dwWidth*2), DXT3 and DXT5 have 8BPP (must become dwWidth*4) :
			decoded.dwRowPitch = _RowBits / 2;
		else
			// All other formats must become dwWidth*BPP/8
			decoded.dwRowPitch = _RowBits / 8;
	
		decoded.SlicePitches[0] = decoded.dwHeight * _RowBits / 8; // Compressed format are half-height
		decoded.dwMipMapLevels = decoded.format.uiMipMapLevels;
	}

    // Calculate a few variables for the first mipmap level (even when this is not a mipmapped texture):
#ifdef INCLUDE_D3DFORMAT_PER_MIPMAP
	decoded.MipMapFormats[0] = dwD3DFormat;
#endif
	decoded.MipMapOffsets[0] = 0;
	decoded.MipMapSlices[0] = decoded.dwDepth;

    // Also calculate mipmap level 1 offset (even when no mipmaps are allowed) as this is used for size :
	decoded.MipMapOffsets[1] = decoded.SlicePitches[0] * decoded.dwDepth; // Offset 2nd level = size of 1st level

    // Since we know how big the first mipmap level is, calculate all following mipmap offsets :
	uint MinSize = 0;
	if (decoded.dwMipMapLevels > 1) {
		uint x = decoded.format.uiUSize;
		uint y = decoded.format.uiVSize;
		uint d = decoded.format.uiPSize;
		if (decoded.bIsCompressed)
			MinSize = 2;
		if (x < MinSize)
			x = MinSize;
		if (y < MinSize)
			y = MinSize;
		// Note : d (depth) is not limited

#ifdef INCLUDE_D3DFORMAT_PER_MIPMAP
		DWORD LocalFormat = dwD3DFormat & ~(X_D3DFORMAT_USIZE_MASK | X_D3DFORMAT_VSIZE_MASK | X_D3DFORMAT_PSIZE_MASK);
#endif
		for (uint v = 1; v < decoded.dwMipMapLevels; v++) {
			// Halve each dimension until it reaches it's lower bound :
			if (x > MinSize)
				x--;
			if (y > MinSize)
				y--;
			if (d > 0)
				d--;

			uint nrslices = 1 << d;
#ifdef INCLUDE_D3DFORMAT_PER_MIPMAP
			decoded.MipMapFormats[v] = LocalFormat || (x << X_D3DFORMAT_USIZE_SHIFT) || (y << X_D3DFORMAT_VSIZE_SHIFT) || (d << X_D3DFORMAT_PSIZE_SHIFT);
#endif
			decoded.MipMapSlices[v] = nrslices;
			decoded.SlicePitches[v] = ((1 << (x + y)) * decoded.dwBPP) / 8;
			// Calculate the next offset by adding the size of this level to the previous offset :
			decoded.MipMapOffsets[v + 1] = decoded.MipMapOffsets[v] + (decoded.SlicePitches[v] * nrslices);
		}
	}

	if (decoded.format.bIsCubeMap)	{
		// Calculate where the next face is located (step over all mipmaps) :
		decoded.dwFacePitch = decoded.MipMapOffsets[decoded.dwMipMapLevels];
		// Align it up :
		decoded.dwFacePitch = RoundUp(decoded.dwFacePitch, X_D3DTEXTURE_CUBEFACE_ALIGNMENT);
	}
	else
		decoded.dwFacePitch = 0; // Cube maps must step this number of bytes per face to skip (this includes all mipmaps for 1 face)

	// 4 for compressed formats, 1 for everything else. Only applies to width & height. Depth can go to 1.
	decoded.dwMinXYValue = 1 << MinSize;
}

inline DWORD GetXboxCommonResourceType(const XTL::X_D3DResource *pXboxResource)
{
	// Don't pass in unassigned Xbox resources
	assert(pXboxResource != NULL);

	DWORD dwCommonType = pXboxResource->Common & X_D3DCOMMON_TYPE_MASK;
	return dwCommonType;
}

inline XTL::X_D3DFORMAT GetXboxPixelContainerFormat(const XTL::X_D3DPixelContainer *pXboxPixelContainer)
{
	// Don't pass in unassigned Xbox pixel container
	assert(pXboxPixelContainer != NULL);

	return (XTL::X_D3DFORMAT)((pXboxPixelContainer->Format & X_D3DFORMAT_FORMAT_MASK) >> X_D3DFORMAT_FORMAT_SHIFT);
}

inline int GetXboxPixelContainerDimensionCount(const XTL::X_D3DPixelContainer *pXboxPixelContainer)
{
	// Don't pass in unassigned Xbox pixel container
	assert(pXboxPixelContainer != NULL);

	return (XTL::X_D3DFORMAT)((pXboxPixelContainer->Format & X_D3DFORMAT_DIMENSION_MASK) >> X_D3DFORMAT_DIMENSION_SHIFT);
}

XTL::X_D3DRESOURCETYPE GetXboxD3DResourceType(const XTL::X_D3DResource *pXboxResource)
{
	DWORD dwCommonType = GetXboxCommonResourceType(pXboxResource);
	switch (dwCommonType)
	{
	case X_D3DCOMMON_TYPE_VERTEXBUFFER:
		return XTL::X_D3DRTYPE_VERTEXBUFFER;
	case X_D3DCOMMON_TYPE_INDEXBUFFER:
		return XTL::X_D3DRTYPE_INDEXBUFFER;
	case X_D3DCOMMON_TYPE_PUSHBUFFER:
		return XTL::X_D3DRTYPE_PUSHBUFFER;
	case X_D3DCOMMON_TYPE_PALETTE:
		return XTL::X_D3DRTYPE_PALETTE;
	case X_D3DCOMMON_TYPE_TEXTURE:
	{
		DWORD Format = ((XTL::X_D3DPixelContainer *)pXboxResource)->Format;
		if (Format & X_D3DFORMAT_CUBEMAP)
			return XTL::X_D3DRTYPE_CUBETEXTURE;

		if (GetXboxPixelContainerDimensionCount((XTL::X_D3DPixelContainer *)pXboxResource) > 2)
			return XTL::X_D3DRTYPE_VOLUMETEXTURE;

		return XTL::X_D3DRTYPE_TEXTURE;
	}
	case X_D3DCOMMON_TYPE_SURFACE:
	{
		if (GetXboxPixelContainerDimensionCount((XTL::X_D3DPixelContainer *)pXboxResource) > 2)
			return XTL::X_D3DRTYPE_VOLUME;

		return XTL::X_D3DRTYPE_SURFACE;
	}
	case X_D3DCOMMON_TYPE_FIXUP:
		return XTL::X_D3DRTYPE_FIXUP;
	}

	return XTL::X_D3DRTYPE_NONE;
}

#if 0
inline bool IsSpecialXboxResource(const XTL::X_D3DResource *pXboxResource)
{
	// Don't pass in unassigned Xbox resources
	assert(pXboxResource != NULL);

	return ((pXboxResource->Data & CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL) == CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL);
}
#endif

// This can be used to determine if resource Data adddresses
// need the MM_SYSTEM_PHYSICAL_MAP bit set or cleared
inline bool IsResourceTypeGPUReadable(const DWORD ResourceType)
{
	switch (ResourceType) {
	case X_D3DCOMMON_TYPE_VERTEXBUFFER:
		return true;
	case X_D3DCOMMON_TYPE_INDEXBUFFER:
		/// assert(false); // Index buffers are not allowed to be registered
		break;
	case X_D3DCOMMON_TYPE_PUSHBUFFER:
		return false;
	case X_D3DCOMMON_TYPE_PALETTE:
		return true;
	case X_D3DCOMMON_TYPE_TEXTURE:
		return true;
	case X_D3DCOMMON_TYPE_SURFACE:
		return true;
	case X_D3DCOMMON_TYPE_FIXUP:
		// assert(false); // Fixup's are not allowed to be registered
		break;
	default:
		CxbxKrnlCleanup("Unhandled resource type");
	}

	return false;
}

void UpdateDepthStencilFlags(const XTL::X_D3DSurface *pXboxSurface)
{
	g_bHasDepthBits = FALSE;
	g_bHasStencilBits = FALSE;
	if (pXboxSurface != NULL) // Prevents D3D error in EMUPATCH(D3DDevice_Clear)
	{
		const XTL::X_D3DFORMAT X_Format = GetXboxPixelContainerFormat(pXboxSurface);
		switch (X_Format)
		{
		case XTL::X_D3DFMT_D16:
		case XTL::X_D3DFMT_LIN_D16: { g_bHasDepthBits = TRUE; break; }

		case XTL::X_D3DFMT_D24S8:
		case XTL::X_D3DFMT_LIN_D24S8: { g_bHasDepthBits = TRUE; g_bHasStencilBits = TRUE; break; }

		case XTL::X_D3DFMT_F24S8:
		case XTL::X_D3DFMT_LIN_F24S8: { g_bHasStencilBits = TRUE; break; }
		}
	}
}

#if 0 // unused
inline bool IsYuvSurface(const XTL::X_D3DResource *pXboxResource)
{
	DWORD dwCommonType = GetXboxCommonResourceType(pXboxResource);
	if (dwCommonType == X_D3DCOMMON_TYPE_SURFACE)
		if (GetXboxPixelContainerFormat((XTL::X_D3DPixelContainer *)pXboxResource) == XTL::X_D3DFMT_YUY2)
			return true;

	return false;
}
#endif

#if 0 // unused
inline bool IsXboxResourceLocked(const XTL::X_D3DResource *pXboxResource)
{
	bool result = !!(pXboxResource->Common & X_D3DCOMMON_ISLOCKED);
	return result;
}
#endif

#if 0 // unused
inline bool IsXboxResourceD3DCreated(const XTL::X_D3DResource *pXboxResource)
{
	bool result = !!(pXboxResource->Common & X_D3DCOMMON_D3DCREATED);
	return result;
}
#endif

#if 0 // unused
static std::map<XTL::X_D3DResource *, XTL::IDirect3DResource8 *> g_XboxToHostResourceMappings;

void SetHostResource(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DResource8 *pHostResource)
{
	g_XboxToHostResourceMappings[pXboxResource] = pHostResource;
}

XTL::IDirect3DResource8 *GetHostResource(XTL::X_D3DResource *pXboxResource)
{
	if (pXboxResource == NULL)
		return nullptr;

	switch (GetXboxCommonResourceType(pXboxResource)) {
	case X_D3DCOMMON_TYPE_PUSHBUFFER:
		return nullptr;
	case X_D3DCOMMON_TYPE_PALETTE:
		return nullptr;
	case X_D3DCOMMON_TYPE_FIXUP:
		return nullptr;
	}

#if 0
	if (IsSpecialXboxResource(pXboxResource))
		return nullptr;
#endif

	XTL::IDirect3DResource8 *result = g_XboxToHostResourceMappings[pXboxResource];
	if (result == nullptr) {
		EmuWarning("Xbox resource has no host resource mapped (yet)");
	}

	return result;
}
#endif

#if 0 // unused
XTL::IDirect3DSurface8 *GetHostSurface(XTL::X_D3DResource *pXboxResource)
{
	if (pXboxResource == NULL)
		return nullptr;

	if (GetXboxCommonResourceType(pXboxResource) != X_D3DCOMMON_TYPE_SURFACE) // Allows breakpoint below
		return nullptr; // TODO : Jet Set Radio Future hits this assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_SURFACE);

	return (XTL::IDirect3DSurface8 *)GetHostResource(pXboxResource);
}
#endif

#if 0 // unused
XTL::IDirect3DBaseTexture8 *GetHostBaseTexture(XTL::X_D3DResource *pXboxResource)
{
	if (pXboxResource == NULL)
		return nullptr;

	if (GetXboxCommonResourceType(pXboxResource) != X_D3DCOMMON_TYPE_TEXTURE) // Allows breakpoint below
		assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_TEXTURE);

	return (XTL::IDirect3DBaseTexture8 *)GetHostResource(pXboxResource);
}
#endif

#if 0 // unused
XTL::IDirect3DTexture8 *GetHostTexture(XTL::X_D3DResource *pXboxResource)
{
	return (XTL::IDirect3DTexture8 *)GetHostBaseTexture(pXboxResource);

	// TODO : Check for 1 face (and 2 dimensions)?
}
#endif

#if 0 // unused
XTL::IDirect3DCubeTexture8 *GetHostCubeTexture(XTL::X_D3DResource *pXboxResource)
{
	return (XTL::IDirect3DCubeTexture8 *)GetHostBaseTexture(pXboxResource);

	// TODO : Check for 6 faces (and 2 dimensions)?
}
#endif

#if 0 // unused
XTL::IDirect3DVolumeTexture8 *GetHostVolumeTexture(XTL::X_D3DResource *pXboxResource)
{
	return (XTL::IDirect3DVolumeTexture8 *)GetHostBaseTexture(pXboxResource);

	// TODO : Check for 3 dimensions?
}
#endif

#if 0 // unused
XTL::IDirect3DIndexBuffer8 *GetHostIndexBuffer(XTL::X_D3DResource *pXboxResource)
{
	if (pXboxResource == NULL)
		return nullptr;

	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_INDEXBUFFER);

	return (XTL::IDirect3DIndexBuffer8 *)GetHostResource(pXboxResource);
}
#endif

#if 0 // unused
XTL::IDirect3DVertexBuffer8 *GetHostVertexBuffer(XTL::X_D3DResource *pXboxResource)
{
	if (pXboxResource == NULL)
		return nullptr;

	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_VERTEXBUFFER);

	return (XTL::IDirect3DVertexBuffer8 *)GetHostResource(pXboxResource);
}
#endif

#if 0 // unused
void SetHostSurface(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DSurface8 *pHostSurface)
{
	assert(pXboxResource != NULL);

	if (GetXboxCommonResourceType(pXboxResource) != X_D3DCOMMON_TYPE_SURFACE) // Allows breakpoint below
		assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_SURFACE); // Hit by LensFlare XDK sample

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostSurface);
}

void SetHostTexture(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DTexture8 *pHostTexture)
{
	assert(pXboxResource != NULL);
	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_TEXTURE);

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostTexture);
}

void SetHostCubeTexture(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DCubeTexture8 *pHostCubeTexture)
{
	assert(pXboxResource != NULL);
	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_TEXTURE);

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostCubeTexture);
}

void SetHostVolumeTexture(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DVolumeTexture8 *pHostVolumeTexture)
{
	assert(pXboxResource != NULL);
	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_TEXTURE);

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostVolumeTexture);
}

void SetHostIndexBuffer(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DIndexBuffer8 *pHostIndexBuffer)
{
	assert(pXboxResource != NULL);
	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_INDEXBUFFER);

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostIndexBuffer);
}

void SetHostVertexBuffer(XTL::X_D3DResource *pXboxResource, XTL::IDirect3DVertexBuffer8 *pHostVertexBuffer)
{
	assert(pXboxResource != NULL);
	assert(GetXboxCommonResourceType(pXboxResource) == X_D3DCOMMON_TYPE_VERTEXBUFFER);

	SetHostResource(pXboxResource, (XTL::IDirect3DResource8 *)pHostVertexBuffer);
}
#endif

void *XTL::GetDataFromXboxResource(XTL::X_D3DResource *pXboxResource)
{
	// Don't pass in unassigned Xbox resources
	if (pXboxResource == NULL)
		return nullptr;

	xbaddr pData = pXboxResource->Data;
	if (pData == NULL)
		return nullptr;

#if 0
	if (IsSpecialXboxResource(pXboxResource))
	{
		switch (pData) {
		case CXBX_D3DRESOURCE_DATA_BACK_BUFFER:
			return nullptr;
		case CXBX_D3DRESOURCE_DATA_RENDER_TARGET:
			return nullptr;
		case CXBX_D3DRESOURCE_DATA_DEPTH_STENCIL:
			return nullptr;
		default:
			CxbxKrnlCleanup("Unhandled special resource type");
		}
	}
#endif

	DWORD dwCommonType = GetXboxCommonResourceType(pXboxResource);
	if (IsResourceTypeGPUReadable(dwCommonType))
		pData |= MM_SYSTEM_PHYSICAL_MAP;

	return (uint08*)pData;
}

void DxbxDetermineSurFaceAndLevelByData(const DecodedPixelContainer PixelJar, OUT UINT &Level, OUT XTL::D3DCUBEMAP_FACES &FaceType)
{
	UINT_PTR ParentData = (UINT_PTR)GetDataFromXboxResource(((XTL::X_D3DSurface*)(PixelJar.pPixelContainer))->Parent);
	UINT_PTR SurfaceData = (UINT_PTR)GetDataFromXboxResource(PixelJar.pPixelContainer);

	// Step to the correct face :
	int f = XTL::D3DCUBEMAP_FACE_POSITIVE_X;
	while (f < XTL::D3DCUBEMAP_FACE_NEGATIVE_Z) {
		if (ParentData >= SurfaceData)
			break;

		ParentData += PixelJar.dwFacePitch;
		f++; // enum can't be increased using FaceType++;
	}
	FaceType = (XTL::D3DCUBEMAP_FACES)f;

	// Step to the correct mipmap level :
	Level = 0;
	while (Level < X_MAX_MIPMAPS) {
		if (ParentData + PixelJar.MipMapOffsets[Level] >= SurfaceData)
			break;

		Level++;
	}
}

bool IsResourceFormatSupported(XTL::X_D3DRESOURCETYPE aResourceType, XTL::D3DFORMAT PCFormat, DWORD aUsage)
{
	bool Result = SUCCEEDED(g_pD3D8->CheckDeviceFormat(
		g_EmuCDPD.HostCreationParameters.AdapterOrdinal, // Note : D3DADAPTER_DEFAULT = 0
		g_EmuCDPD.HostCreationParameters.DeviceType, // Note : D3DDEVTYPE_HAL = 1, D3DDEVTYPE_REF = 2, D3DDEVTYPE_SW = 3
		g_EmuCDPD.HostPresentationParameters.BackBufferFormat,
		aUsage,
		(XTL::D3DRESOURCETYPE)aResourceType, // Note : X_D3DRTYPE_SURFACE up to X_D3DRTYPE_INDEXBUFFER values matches host D3D
		PCFormat));
	return Result;
}

namespace XTL {

D3DFORMAT DxbxXB2PC_D3DFormat(X_D3DFORMAT X_Format, X_D3DRESOURCETYPE aResourceType, IN OUT DWORD &aUsage, OUT bool &ConversionToARGBNeeded)
{
	// Convert Format (Xbox->PC)
	D3DFORMAT Result = EmuXB2PC_D3DFormat(X_Format);

	// Check device caps :
	if (IsResourceFormatSupported(aResourceType, Result, aUsage))
		return Result;

	// Can we convert this format to ARGB?
	ConversionToARGBNeeded = EmuXBFormatCanBeConvertedToARGB(X_Format);
	if (ConversionToARGBNeeded) {
		return D3DFMT_A8R8G8B8;
	}

	D3DFORMAT FirstResult = Result;
	switch (Result) {
	case D3DFMT_P8: 
		// Note : Allocate a BPP-identical format instead of P8 (which is nearly never supported natively),
		// so that we at least have a BPP-identical format. Conversion to ARGB is done via bConvertToARGB in
		// in CxbxUpdateTexture :
		Result = D3DFMT_L8;
		break;
	case D3DFMT_D16:
		switch (aResourceType) {
		case X_D3DRTYPE_TEXTURE:
		case X_D3DRTYPE_VOLUMETEXTURE:
		case X_D3DRTYPE_CUBETEXTURE: {
			if ((aUsage & X_D3DUSAGE_DEPTHSTENCIL) > 0) {
				Result = D3DFMT_D16_LOCKABLE;
				// ATI Fix : Does this card support D16 DepthStencil textures ?
				if (!IsResourceFormatSupported(aResourceType, Result, aUsage)) {
					// If not, fall back to a format that's the same size and is most probably supported :
					Result = D3DFMT_R5G6B5; // Note : If used in shaders, this will give unpredictable channel mappings!

					// Since this cannot longer be created as a DepthStencil, reset the usage flag :
					aUsage &= ~X_D3DUSAGE_DEPTHSTENCIL; // TODO : This asks for a testcase!
					LOG_TEST_CASE("fallback to D3DFMT_R5G6B5");
				}
			}
			else
				Result = D3DFMT_R5G6B5; // also CheckDeviceMultiSampleType

			break;
		}
		case X_D3DRTYPE_SURFACE:
			Result = D3DFMT_D16_LOCKABLE;
			break;
		}
		break;
	case D3DFMT_D24S8:
		switch (aResourceType) {
		case X_D3DRTYPE_TEXTURE:
		case X_D3DRTYPE_VOLUMETEXTURE:
		case X_D3DRTYPE_CUBETEXTURE: {
			Result = D3DFMT_A8R8G8B8;
			// Since this cannot longer be created as a DepthStencil, reset the usage flag :
			aUsage &= ~X_D3DUSAGE_DEPTHSTENCIL; // TODO : This asks for a testcase!
			LOG_TEST_CASE("fallback to D3DFMT_A8R8G8B8");
			break;
		}
		}
		break;
	case D3DFMT_X1R5G5B5:
		// TODO: HACK: This texture format fails on some newer hardware
		Result = D3DFMT_R5G6B5;
		break;
	case D3DFMT_V16U16:
		// HACK. This fixes NoSortAlphaBlend (after B button - z-replace) on nvidia:
		Result = D3DFMT_A8R8G8B8;
		break;
	case D3DFMT_YUY2:
		// TODO : Is this still neccessary?
		Result = D3DFMT_V8U8; // Use another format that's also 16 bits wide

		break;
	}

	if (FirstResult != Result) {
		if (IsResourceFormatSupported(aResourceType, Result, aUsage))
			EmuWarning("%s is an unsupported format for %s! Allocating %s",
				X_D3DFORMAT2String(X_Format).c_str(),
				X_D3DRESOURCETYPE2String(aResourceType).c_str(),
				D3DFORMAT2PCHAR(Result));
		else
			CxbxKrnlCleanup("%s is an unsupported format for %s!\nFallback %s is also not supported!",
				X_D3DFORMAT2String(X_Format).c_str(),
				X_D3DRESOURCETYPE2String(aResourceType).c_str(),
				D3DFORMAT2PCHAR(Result));
	}

	return Result;
}

} // end of namespace XTL;

#if 0 // unused
inline XTL::X_D3DPALETTESIZE GetXboxPaletteSize(const XTL::X_D3DPalette *pPalette)
{
	XTL::X_D3DPALETTESIZE PaletteSize = (XTL::X_D3DPALETTESIZE)
		((pPalette->Common & X_D3DPALETTE_COMMON_PALETTESIZE_MASK) >> X_D3DPALETTE_COMMON_PALETTESIZE_SHIFT);

	return PaletteSize;
}
#endif

#if 0 // unused
int GetD3DResourceRefCount(XTL::IDirect3DResource8 *EmuResource)
{
	if (EmuResource != nullptr)
	{
		// Get actual reference count by increasing it using AddRef,
		// and relying on the return value of Release (which is
		// probably more reliable than AddRef)
		EmuResource->AddRef();
		return EmuResource->Release();
	}

	return 0;
}
#endif

// TODO : Avoid returning allocations outside Xbox heap, back to Xbox
XTL::X_D3DSurface *EmuNewD3DSurface()
{
	XTL::X_D3DSurface *result = (XTL::X_D3DSurface *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DSurface));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_SURFACE | 1; // Set refcount to 1
	return result;
}

#if 0 // No longer used (now we stopped patching D3DDevice_Create* functions)
XTL::X_D3DTexture *EmuNewD3DTexture()
{
	XTL::X_D3DTexture *result = (XTL::X_D3DTexture *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DTexture));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_TEXTURE | 1; // Set refcount to 1
	return result;
}

XTL::X_D3DVolumeTexture *EmuNewD3DVolumeTexture()
{
	XTL::X_D3DVolumeTexture *result = (XTL::X_D3DVolumeTexture *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DVolumeTexture));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_TEXTURE | 1; // Set refcount to 1
	return result;
}

XTL::X_D3DCubeTexture *EmuNewD3DCubeTexture()
{
	XTL::X_D3DCubeTexture *result = (XTL::X_D3DCubeTexture *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DCubeTexture));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_TEXTURE | 1; // Set refcount to 1
	return result;
}

XTL::X_D3DIndexBuffer *EmuNewD3DIndexBuffer()
{
	XTL::X_D3DIndexBuffer *result = (XTL::X_D3DIndexBuffer *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DIndexBuffer));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_INDEXBUFFER | 1; // Set refcount to 1
	return result;
}

XTL::X_D3DVertexBuffer *EmuNewD3DVertexBuffer()
{
	XTL::X_D3DVertexBuffer *result = (XTL::X_D3DVertexBuffer *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DVertexBuffer));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_VERTEXBUFFER | 1; // Set refcount to 1
	return result;
}

XTL::X_D3DPalette *EmuNewD3DPalette()
{
	XTL::X_D3DPalette *result = (XTL::X_D3DPalette *)g_MemoryManager.AllocateZeroed(1, sizeof(XTL::X_D3DPalette));
	result->Common = X_D3DCOMMON_D3DCREATED | X_D3DCOMMON_TYPE_PALETTE | 1; // Set refcount to 1
	return result;
}
#endif

#if 1 // temporarily used by ConvertHostSurfaceHeaderToXbox() and WndMain::LoadGameLogo()
// TODO : Move to own file
VOID XTL::CxbxSetPixelContainerHeader
(
	XTL::X_D3DPixelContainer* pPixelContainer,
	DWORD				Common,
	UINT				Width,
	UINT				Height,
	UINT				Levels,
	XTL::X_D3DFORMAT	Format,
	UINT				Dimensions,
	UINT				Pitch
)
{
	// Set X_D3DResource field(s) :
	pPixelContainer->Common = Common;
	// DON'T SET pPixelContainer->Data
	// DON'T SET pPixelContainer->Lock

	// Are Width and Height both a power of two?
	DWORD l2w; _BitScanReverse(&l2w, Width); // MSVC intrinsic; GCC has __builtin_clz
	DWORD l2h; _BitScanReverse(&l2h, Height);
	if (((1 << l2w) == Width) && ((1 << l2h) == Height)) {
		Width = Height = Pitch = 1; // When setting Format, clear Size field
	}
	else {
		l2w = l2h = 0; // When setting Size, clear D3DFORMAT_USIZE and VSIZE
	}

	// TODO : Must this be set using Usage / Pool / something else?
	const int Depth = 1;

	// Set X_D3DPixelContainer field(s) :
	pPixelContainer->Format = 0
		| ((Dimensions << X_D3DFORMAT_DIMENSION_SHIFT) & X_D3DFORMAT_DIMENSION_MASK)
		| (((DWORD)Format << X_D3DFORMAT_FORMAT_SHIFT) & X_D3DFORMAT_FORMAT_MASK)
		| ((Levels << X_D3DFORMAT_MIPMAP_SHIFT) & X_D3DFORMAT_MIPMAP_MASK)
		| ((l2w << X_D3DFORMAT_USIZE_SHIFT) & X_D3DFORMAT_USIZE_MASK)
		| ((l2h << X_D3DFORMAT_VSIZE_SHIFT) & X_D3DFORMAT_VSIZE_MASK)
		| ((Depth << X_D3DFORMAT_PSIZE_SHIFT) & X_D3DFORMAT_PSIZE_MASK)
		;
	pPixelContainer->Size = 0
		| (((Width - 1) /*X_D3DSIZE_WIDTH_SHIFT*/) & X_D3DSIZE_WIDTH_MASK)
		| (((Height - 1) << X_D3DSIZE_HEIGHT_SHIFT) & X_D3DSIZE_HEIGHT_MASK)
		| (((Pitch - 1) << X_D3DSIZE_PITCH_SHIFT) & X_D3DSIZE_PITCH_MASK)
		;
}

// TODO : Move to own file
// HACK : Approximate the format
void ConvertHostSurfaceHeaderToXbox
(
	XTL::IDirect3DSurface8 *pHostSurface,
	XTL::X_D3DPixelContainer* pPixelContainer
)
{
	XTL::D3DSURFACE_DESC HostSurfaceDesc;

	pHostSurface->GetDesc(&HostSurfaceDesc);
	const uint MipMapLevels = 1; // ??
	XTL::X_D3DFORMAT X_Format = EmuPC2XB_D3DFormat(HostSurfaceDesc.Format);
	const uint Dimensions = 2;
	uint Pitch = HostSurfaceDesc.Width * EmuXBFormatBitsPerPixel(X_Format) / 8;

	XTL::CxbxSetPixelContainerHeader(pPixelContainer,
		pPixelContainer->Common,
		HostSurfaceDesc.Width,
		HostSurfaceDesc.Height,
		MipMapLevels,
		X_Format,
		Dimensions,
		Pitch);
}
#endif

// TODO : Move to own file
UINT CxbxFormatAndWidthToRowSizeInBytes(
	XTL::X_D3DFORMAT X_Format,
	UINT uWidth
)
{
	// Calculate the size of a row, based on the number of bits per pixel of the given format
	DWORD dwBPP = EmuXBFormatBitsPerPixel(X_Format);
	UINT uPitch = RoundUp((uWidth * dwBPP) / 8, X_D3DTEXTURE_PITCH_ALIGNMENT);

	// Compressed formats encode 4 lines per row, so adjust pitch accordingly
	if (EmuXBFormatIsCompressed(X_Format))
		uPitch *= 4;

	return uPitch;
}

// TODO : Move to own file
VOID CxbxGetPixelContainerMeasures
(
	XTL::X_D3DPixelContainer *pPixelContainer,
	DWORD dwLevel,
	UINT *pWidth,
	UINT *pHeight,
	UINT *pPitch,
	UINT *pSize = nullptr
)
{
	DWORD Size = pPixelContainer->Size;
	XTL::X_D3DFORMAT X_Format = GetXboxPixelContainerFormat(pPixelContainer);

	if (Size != 0) {
		*pWidth = ((Size & X_D3DSIZE_WIDTH_MASK) /* >> X_D3DSIZE_WIDTH_SHIFT*/) + 1;
		*pHeight = ((Size & X_D3DSIZE_HEIGHT_MASK) >> X_D3DSIZE_HEIGHT_SHIFT) + 1;
		*pPitch = (((Size & X_D3DSIZE_PITCH_MASK) >> X_D3DSIZE_PITCH_SHIFT) + 1) * X_D3DTEXTURE_PITCH_ALIGNMENT;
	}
	else {
		DWORD l2w = (pPixelContainer->Format & X_D3DFORMAT_USIZE_MASK) >> X_D3DFORMAT_USIZE_SHIFT;
		DWORD l2h = (pPixelContainer->Format & X_D3DFORMAT_VSIZE_MASK) >> X_D3DFORMAT_VSIZE_SHIFT;

		*pHeight = 1 << l2h;
		*pWidth = 1 << l2w;
		*pPitch = CxbxFormatAndWidthToRowSizeInBytes(X_Format, *pWidth);
	}

	if (pSize) {
		*pSize = *pHeight * *pPitch;
		// Compressed formats encode 4 lines per row, so adjust size accordingly
		if (EmuXBFormatIsCompressed(X_Format)) {
			*pSize /= 4;
		}
	}
#if 0 // Enable to dump results

	{
		DbgPrintf("pPixelContainer = %p\n", pPixelContainer);
		DbgPrintf("dwLevel = %d\n", dwLevel);
		DbgPrintf("*pWidth = %d\n", *pWidth);
		DbgPrintf("*pHeight = %d\n", *pHeight);
		DbgPrintf("*pPitch = %d\n", *pPitch);
		DbgPrintf("*pSize = %d\n", *pSize);
	}
#endif
}

// TODO : Move to own file
uint8 *XTL::ConvertD3DTextureToARGB(
	XTL::X_D3DPixelContainer *pXboxPixelContainer,
	uint8 *pSrc,
	int *pWidth, int *pHeight
)
{
	XTL::X_D3DFORMAT X_Format = GetXboxPixelContainerFormat(pXboxPixelContainer);
	const XTL::FormatToARGBRow ConvertRowToARGB = EmuXBFormatComponentConverter(X_Format);
	if (ConvertRowToARGB == nullptr)
		return nullptr; // Unhandled conversion

	uint SrcPitch, SrcSize;
	CxbxGetPixelContainerMeasures(
		pXboxPixelContainer,
		0, // dwLevel
		(UINT*)pWidth,
		(UINT*)pHeight,
		&SrcPitch,
		&SrcSize
	);

	int DestPitch = *pWidth * sizeof(DWORD);
	uint8 *pDest = (uint8 *)malloc(DestPitch * *pHeight);

	uint8 *unswizleBuffer = nullptr;
	if (XTL::EmuXBFormatIsSwizzled(X_Format)) {
		unswizleBuffer = (uint8*)malloc(SrcPitch * *pHeight); // TODO : Reuse buffer when performance is important
		// First we need to unswizzle the texture data
		XTL::EmuUnswizzleRect(
			pSrc, *pWidth, *pHeight, 1, unswizleBuffer,
			SrcPitch, EmuXBFormatBytesPerPixel(X_Format)
		);
		// Convert colors from the unswizzled buffer
		pSrc = unswizleBuffer;
	}

	DWORD SrcRowOff = 0;
	uint8 *pDestRow = pDest;
	if (EmuXBFormatIsCompressed(X_Format)) {
		// All compressed formats (DXT1, DXT3 and DXT5) encode blocks of 4 pixels on 4 lines
		for (int y = 0; y < *pHeight; y+=4) {
			*(int*)pDestRow = DestPitch; // Dirty hack, to avoid an extra parameter to all conversion callbacks
			ConvertRowToARGB(pSrc + SrcRowOff, pDestRow, *pWidth);
			SrcRowOff += SrcPitch;
			pDestRow += DestPitch * 4;
		}
	}
	else {
		while (SrcRowOff < SrcSize) {
			ConvertRowToARGB(pSrc + SrcRowOff, pDestRow, *pWidth);
			SrcRowOff += SrcPitch;
			pDestRow += DestPitch;
		}
	}

	if (unswizleBuffer)
		free(unswizleBuffer);

	return pDest;
}

void DeriveXboxD3DDeviceAddresses()
{
	// Read the Xbox g_pDevice pointer into our D3D Device object 
	DWORD *Derived_Xbox_D3DDevice = *(DWORD **)XTL::Xbox_pD3DDevice;
	if (Derived_Xbox_D3DDevice == NULL)
		// As long as it isn't set, don't guess :
		return;

	if (XTL::Xbox_D3DDevice == Derived_Xbox_D3DDevice)
		return;

	// Derive Xbox_D3DDevice member addresses in incrementing order of occurence 
	XTL::Xbox_D3DDevice = Derived_Xbox_D3DDevice;
	DbgPrintf("INIT: 0x%p -> Xbox_D3DDevice (Derived)\n", XTL::Xbox_D3DDevice);

#define DeriveD3DDeviceMember(offsetof_Xbox_D3DDevice_m, type, variable) \
	if (offsetof_Xbox_D3DDevice_m > 0) { \
		variable = (type)((uint8 *)XTL::Xbox_D3DDevice + offsetof_Xbox_D3DDevice_m); \
		DbgPrintf("INIT: 0x%p -> " ## #variable ## " (Derived)\n", (void *)variable); \
	} \
	else { \
		variable = NULL; \
		DbgPrintf("INIT: Missing D3D : " ## #variable ## "\n"); \
	}

	// Determine the active vertex index
	// This reads from g_pDevice->m_IndexBase in Xbox D3D
	DeriveD3DDeviceMember(7 * sizeof(DWORD), DWORD *, XTL::Xbox_D3DDevice_m_IndexBase);
	DeriveD3DDeviceMember(XTL::offsetof_Xbox_D3DDevice_m_PixelShader, DWORD *, XTL::Xbox_D3DDevice_m_PixelShader);
	DeriveD3DDeviceMember(XTL::offsetof_Xbox_D3DDevice_m_pTextures, XTL::X_D3DBaseTexture **, XTL::Xbox_D3DDevice_m_pTextures);
	DeriveD3DDeviceMember(XTL::offsetof_Xbox_D3DDevice_m_pPalettes, XTL::X_D3DPalette **, XTL::Xbox_D3DDevice_m_pPalettes);
	DeriveD3DDeviceMember(XTL::offsetof_Xbox_D3DDevice_m_pRenderTarget, XTL::X_D3DSurface **, XTL::Xbox_D3DDevice_m_pRenderTarget);
	DeriveD3DDeviceMember(XTL::offsetof_Xbox_D3DDevice_m_pDepthStencil, XTL::X_D3DSurface **, XTL::Xbox_D3DDevice_m_pDepthStencil);
}

bool IsAddressAllocated(void *addr)
{
	MEMORY_BASIC_INFORMATION MemInfo;
	VirtualQuery(addr, &MemInfo, sizeof(MemInfo));
	return (MemInfo.State & MEM_COMMIT) > 0;
}

bool g_bIntegrityChecking = false;
int thread_local recursion = 0;
#ifdef _DEBUG
void CxbxCheckIntegrity()
{
	if (g_bIntegrityChecking) {
		if (recursion++ == 0) {
			_CrtCheckMemory();
			if (XTL::Xbox_D3DDevice_m_pTextures) {
				for (int Stage = 0; Stage < X_D3DTSS_STAGECOUNT; Stage++) {
					void *Addr;

					Addr = XTL::GetXboxBaseTexture(Stage);
					if (Addr && !IsAddressAllocated(Addr))
						CxbxPopupMessage("Texture %d got damaged to 0x%p!", Stage, Addr);

					Addr = XTL::GetXboxPalette(Stage);
					if (Addr && !IsAddressAllocated(Addr))
						CxbxPopupMessage("Palette %d got damaged to 0x%p!", Stage, Addr);
				}
			}
		}
		recursion--;
	}
}
#endif

// TODO : Move to own file
void CxbxUpdateTextureStages()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	// HACK! DeriveXboxD3DDeviceAddresses should be called as soon as Xbox CreateDevice has set g_pDevice (to g_Device)
	// TODO : Move this call to the appropriate time (so it won't be repeated as often as now, here in CxbxUpdateTextureStages)
	DeriveXboxD3DDeviceAddresses();

	for (int Stage = 0; Stage < X_D3DTSS_STAGECOUNT; Stage++) {
		XTL::IDirect3DBaseTexture8 *pHostBaseTexture = nullptr;
		if (g_FillModeOverride == 0) {
			auto XboxPalette = XTL::GetXboxPalette(Stage);
			XTL::X_D3DBaseTexture *XboxBaseTexture = XTL::GetXboxBaseTexture(Stage);

			if (XboxBaseTexture != NULL) {
				if (!IsAddressAllocated(XboxBaseTexture))
					EmuWarning("CxbxUpdateTextureStages read a rogue pointer! [%d]=0x%p", Stage, XboxBaseTexture);
				else {
					DWORD *PaletteColors = NULL;
					if (XboxPalette != NULL) {
						if (!IsAddressAllocated(XboxPalette))
							EmuWarning("CxbxUpdateTextureStages read a rogue pointer! [%d]=0x%p", Stage, XboxPalette);
						else
							PaletteColors = (DWORD*)GetDataFromXboxResource(XboxPalette);
					}

					pHostBaseTexture = CxbxUpdateTexture(XboxBaseTexture, PaletteColors);
				}
			}
		}

		HRESULT hRet = g_pD3DDevice8->SetTexture(Stage, pHostBaseTexture);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetTexture");
	}
}

// TODO : Move to own file
XTL::IDirect3DSurface8 *CxbxUpdateSurface(const XTL::X_D3DSurface *pXboxSurface)
{
	if (pXboxSurface == NULL)
		return nullptr;

/* Note : I don't this an update of this surface's Parent is needed, but if it is, here's how :
	if (pXboxSurface->Parent != NULL)
		(XTL::IDirect3DSurface8 *)CxbxUpdateTexture((XTL::X_D3DPixelContainer *)pXboxSurface->Parent, NULL);

*/
	return (XTL::IDirect3DSurface8 *)CxbxUpdateTexture((XTL::X_D3DPixelContainer *)pXboxSurface, NULL);
}

// TODO : Move to own file
void CxbxUpdateActiveRenderTarget()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	XTL::X_D3DSurface *pActiveXboxRenderTarget = GetXboxRenderTarget();
	XTL::X_D3DSurface *pActiveXboxDepthStencil = GetXboxDepthStencil();
	XTL::IDirect3DSurface8 *g_pActiveHostRenderTarget = CxbxUpdateSurface(pActiveXboxRenderTarget);
	XTL::IDirect3DSurface8 *g_pActiveHostDepthStencil = CxbxUpdateSurface(pActiveXboxDepthStencil);

	HRESULT hRet = g_pD3DDevice8->SetRenderTarget(g_pActiveHostRenderTarget, g_pActiveHostDepthStencil);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderTarget");

	// This tries to fix VolumeFog on ATI :
	if (FAILED(hRet))
	{
		// TODO : Maybe some info : http://xboxforums.create.msdn.com/forums/t/2124.aspx
		EmuWarning("SetRenderTarget failed! Trying ATI fix");
		hRet = g_pD3DDevice8->SetRenderTarget(g_pActiveHostRenderTarget, nullptr);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderTarget [second]");
	}
}

void CxbxUpdateNativeD3DResources()
{
	/* Dxbx has :
	DxbxUpdateActiveVertexShader();
	DxbxUpdateActiveTextures();
	DxbxUpdateActivePixelShader();
	DxbxUpdateDeferredStates(); // BeginPush sample shows us that this must come *after* texture update!
	DxbxUpdateActiveVertexBufferStreams();
	DxbxUpdateActiveRenderTarget();
	*/
	CxbxUpdateTextureStages();
	XTL::DxbxUpdateActivePixelShader();
	XTL::DxbxUpdateDeferredStates(); // BeginPush sample shows us that this must come *after* texture update!
	CxbxUpdateActiveRenderTarget(); // Make sure the correct output surfaces are used
	// TODO : Transfer matrices (projection/model/world view) from Xbox to Host using GetTransform or D3D_Device member pointers
}

void CxbxInternalSetRenderState
(
	const char *Caller,
	XTL::X_D3DRENDERSTATETYPE XboxRenderState,
	DWORD XboxValue
)
{
	LOG_FUNC_INIT(Caller)

	if (Caller) {
		LOG_FUNC_BEGIN_NO_INIT
			LOG_FUNC_ARG(XboxRenderState)
			LOG_FUNC_ARG(XboxValue)
			LOG_FUNC_END;
	}

	LOG_FINIT

	if (XboxRenderState > XTL::X_D3DRS_LAST) {
		// TODO : EmuWarning("Xbox would fail - emulate how");
		return;
	}

	// Set this value into the RenderState structure too (so other code will read the new current value) :
	SetXboxRenderState(XboxRenderState, XboxValue);
	// TODO : Update the D3D DirtyFlags too?

	// Don't set deferred render states at this moment (we'll transfer them at drawing time)
	if (XboxRenderState >= XTL::X_D3DRS_DEFERRED_FIRST && XboxRenderState <= XTL::X_D3DRS_DEFERRED_LAST)
		return;

	// Transfer over the render state to PC :
	DWORD PCValue = XTL::Dxbx_SetRenderState(XboxRenderState, XboxValue);

	// Keep log identical, show the value that's being forwarded to PC :
	{
		const XTL::RenderStateInfo &Info = XTL::GetDxbxRenderStateInfo(XboxRenderState);

		if (Info.PC == (XTL::D3DRENDERSTATETYPE)0) // D3DRS_UNSUPPORTED
			EmuWarning("RenderState (%s, 0x%.08X) is unsupported!",
				Info.S + 2,  // Skip "X_" prefix
				PCValue);
		else
			if (PCValue != XboxValue)
				DbgPrintf("  Set %s := 0x%.08X (converted from Xbox)\n",
					Info.S + 2,  // Skip "X_" prefix
					PCValue);
			else
				DbgPrintf("  Set %s := 0x%.08X\n",
					Info.S + 2,  // Skip "X_" prefix
					PCValue);
	}
}

void CxbxInternalSetTextureStageState
(
	const char *Caller,
	DWORD Stage,
	XTL::X_D3DTEXTURESTAGESTATETYPE XboxTextureStageState,
	DWORD XboxValue
)
{
	LOG_FUNC_INIT(Caller)

	if (Caller) {
		LOG_FUNC_BEGIN_NO_INIT
			LOG_FUNC_ARG(Stage)
			LOG_FUNC_ARG(XboxTextureStageState)
			LOG_FUNC_ARG(XboxValue)
			LOG_FUNC_END;
	}

	LOG_FINIT

	if (Stage >= X_D3DTSS_STAGECOUNT) {
		// TODO : EmuWarning("Xbox would fail - emulate how");
		return;
	}

	if (XboxTextureStageState >= XTL::X_D3DTSS_UNSUPPORTED) {
		// TODO : EmuWarning("Xbox would fail - emulate how");
		return;
	}

	// Set this value into the TextureState structure too (so other code will read the new current value)
	XTL::SetXboxTextureStageState(Stage, XboxTextureStageState, XboxValue);
	// TODO : Update the D3D DirtyFlags too?

	// Transfer over the texture stage state to PC :
	DWORD PCValue = XTL::Cxbx_SetTextureStageState(Stage, XboxTextureStageState, XboxValue);

	// Keep log identical, show the value that's being forwarded to PC :
	{
		const XTL::TextureStageStateInfo &Info = XTL::DxbxTextureStageStateInfo[XboxTextureStageState];

		if (Info.PC == (XTL::D3DSAMPLERSTATETYPE)0) // D3DSAMP_UNSUPPORTED
			EmuWarning("TextureStageState (%s, 0x%.08X) not supported!",
				Info.S + 2,  // Skip "X_" prefix
				PCValue);
		else
			if (PCValue != XboxValue)
				DbgPrintf("  Set %s := 0x%.08X (converted from Xbox)\n",
					Info.S + 2,  // Skip "X_" prefix
					PCValue);
			else
				DbgPrintf("  Set %s := 0x%.08X\n",
					Info.S + 2,  // Skip "X_" prefix
					PCValue);
	}
}

// Direct3D initialization (called before emulation begins)
VOID XTL::EmuD3DInit()
{
	// create the create device proxy thread
	{
		DWORD dwThreadId = 0;

		HANDLE hThread = CreateThread(nullptr, 0, EmuCreateDeviceProxy, nullptr, 0, &dwThreadId);
		DbgPrintf("INIT: Created CreateDevice proxy thread. Handle : 0x%X, ThreadId : [0x%.4X]\n", hThread, dwThreadId);
		// Ported from Dxbx :
		// If possible, assign this thread to another core than the one that runs Xbox1 code :
		SetThreadAffinityMask(&dwThreadId, g_CPUOthers);
	}

	// create Direct3D8 and retrieve caps
    {
        // xbox Direct3DCreate8 returns "1" always, so we need our own ptr
        g_pD3D8 = Direct3DCreate8(D3D_SDK_VERSION);
        if(g_pD3D8 == nullptr)
            CxbxKrnlCleanup("Could not initialize Direct3D8!");

		g_D3DDeviceType = (g_XBVideo.GetDirect3DDevice() == 0) ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF;
        g_pD3D8->GetDeviceCaps(g_XBVideo.GetDisplayAdapter(), g_D3DDeviceType, &g_D3DCaps);
		g_pD3D8->GetAdapterDisplayMode(g_XBVideo.GetDisplayAdapter(), &g_D3DDisplayMode);
	}

	// create default device
	{
		// Defaults, probably overwritten by settings below
		XboxD3DPresentParameters.BackBufferWidth = 640;
		XboxD3DPresentParameters.BackBufferHeight = 480;
		XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_A8R8G8B8;
		XboxD3DPresentParameters.BackBufferCount = 1;
		// XboxD3DPresentParameters.MultiSampleType = (X_D3DMULTISAMPLE_TYPE)?;
		XboxD3DPresentParameters.SwapEffect = g_XBVideo.GetVSync() ? X_D3DSWAPEFFECT_COPY_VSYNC : X_D3DSWAPEFFECT_DISCARD;
		// XboxD3DPresentParameters.hDeviceWindow = (HWND)?;
		XboxD3DPresentParameters.Windowed = FALSE; // TRUE?
		XboxD3DPresentParameters.EnableAutoDepthStencil = TRUE;
		XboxD3DPresentParameters.AutoDepthStencilFormat = X_D3DFMT_D24S8;
		// XboxD3DPresentParameters.Flags = (DWORD)?;
		// XboxD3DPresentParameters.FullScreen_RefreshRateInHz = (UINT)?;
		// XboxD3DPresentParameters.FullScreen_PresentationInterval = (UINT)?;
		// Note : The Windows DirectX8 variant ends here
		// XboxD3DPresentParameters.BufferSurfaces = (void *)[?,?,?]; // IDirect3DSurface8
		// XboxD3DPresentParameters.DepthStencilSurface = (void *)?; // IDirect3DSurface8

		// retrieve resolution from configuration
		if (!g_XBVideo.GetFullscreen()) { // g_EmuCDPD.HostPresentationParameters.Windowed
			sscanf(g_XBVideo.GetVideoResolution(), "%u x %u",
				&XboxD3DPresentParameters.BackBufferWidth,
				&XboxD3DPresentParameters.BackBufferHeight);

			if (g_D3DDisplayMode.Format == D3DFMT_X8R8G8B8)
				XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_A8R8G8B8; // Xbox often wants this, good enough
			else
				XboxD3DPresentParameters.BackBufferFormat = EmuPC2XB_D3DFormat(g_D3DDisplayMode.Format);

			XboxD3DPresentParameters.FullScreen_RefreshRateInHz = 0; // g_D3DDisplayMode.RefreshRate ??
		}
		else {
			char szBackBufferFormat[16];

			sscanf(g_XBVideo.GetVideoResolution(), "%u x %u %*dbit %s (%u hz)",
				&XboxD3DPresentParameters.BackBufferWidth,
				&XboxD3DPresentParameters.BackBufferHeight,
				szBackBufferFormat,
				&XboxD3DPresentParameters.FullScreen_RefreshRateInHz);

			if (strcmp(szBackBufferFormat, "x1r5g5b5") == 0)
				XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_X1R5G5B5;
			else if (strcmp(szBackBufferFormat, "r5g6r5") == 0)
				XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_R5G6B5;
			else if (strcmp(szBackBufferFormat, "x8r8g8b8") == 0)
				XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_X8R8G8B8;
			else // if (strcmp(szBackBufferFormat, "a8r8g8b8") == 0)
				XboxD3DPresentParameters.BackBufferFormat = X_D3DFMT_A8R8G8B8;
		}

		Cxbx_Direct3D_CreateDevice();
    }
}

// cleanup Direct3D
VOID XTL::EmuD3DCleanup()
{
    EmuDInputCleanup();
}

// enumeration procedure for locating display device GUIDs
static BOOL WINAPI EmuEnumDisplayDevices(GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm)
{
    static DWORD dwEnumCount = 0;

    if(dwEnumCount++ == g_XBVideo.GetDisplayAdapter()+1)
    {
        g_hMonitor = hm;
        dwEnumCount = 0;
        if(lpGUID != nullptr)
            g_ddguid = *lpGUID;
        else
			g_ddguid = { 0 };

        return FALSE;
    }

    return TRUE;
}

// window message processing thread
static DWORD WINAPI EmuRenderWindow(LPVOID lpVoid)
{
	DbgPrintf("D3D8: Message-Pump thread started\n");

    // register window class
    {
        LOGBRUSH logBrush = {BS_SOLID, RGB(0,0,0)};

        g_hBgBrush = CreateBrushIndirect(&logBrush);

        WNDCLASSEX wc =
        {
            sizeof(WNDCLASSEX), // cbSize
            CS_CLASSDC, // style
            EmuMsgProc, // lpfnWndProc
            0, // cbClsExtra
			0, // cbWndExtra
			GetModuleHandle(nullptr), // hInstance
			NULL, // hIcon - TODO : LoadIcon(hmodule, ?)
            LoadCursor(NULL, IDC_ARROW), // hCursor
            (HBRUSH)g_hBgBrush, // hbrBackground
			nullptr, // lpszMenuName
            "CxbxRender", // lpszClassName
            NULL // hIconSm
        };

        RegisterClassEx(&wc);
    }

    // create the window
    {
        DWORD dwStyle = (g_XBVideo.GetFullscreen() || (CxbxKrnl_hEmuParent == 0))? WS_OVERLAPPEDWINDOW : WS_CHILD;

        int nTitleHeight  = GetSystemMetrics(SM_CYCAPTION);
        int nBorderWidth  = GetSystemMetrics(SM_CXSIZEFRAME);
        int nBorderHeight = GetSystemMetrics(SM_CYSIZEFRAME);

        int x = 100, y = 100, nWidth = 640, nHeight = 480;

        nWidth  += nBorderWidth*2;
        nHeight += nBorderHeight*2 + nTitleHeight;

        sscanf(g_XBVideo.GetVideoResolution(), "%d x %d", &nWidth, &nHeight);

        if(g_XBVideo.GetFullscreen())
        {
            x = y = nWidth = nHeight = 0;
            dwStyle = WS_POPUP;
        }

        HWND hwndParent = GetDesktopWindow();

        if(!g_XBVideo.GetFullscreen())
        {
            hwndParent = CxbxKrnl_hEmuParent;
        }

        g_hEmuWindow = CreateWindow
        (
            "CxbxRender", "Cxbx-Reloaded",
            dwStyle, x, y, nWidth, nHeight,
            hwndParent, NULL, GetModuleHandle(NULL), NULL
        );
    }

    ShowWindow(g_hEmuWindow, ((CxbxKrnl_hEmuParent == NULL) || g_XBVideo.GetFullscreen()) ? SW_SHOWDEFAULT : SW_SHOWMAXIMIZED);
    UpdateWindow(g_hEmuWindow);

    if(!g_XBVideo.GetFullscreen() && (CxbxKrnl_hEmuParent != NULL))
    {
        SetFocus(CxbxKrnl_hEmuParent);
    }

    // initialize direct input
    if(!XTL::EmuDInputInit())
        CxbxKrnlCleanup("Could not initialize DirectInput!");

    DbgPrintf("D3D8: Message-Pump thread is running\n");

    SetFocus(g_hEmuWindow);

    DbgConsole *dbgConsole = new DbgConsole();

    // message processing loop
    {
		MSG msg = { 0 };

        bool lPrintfOn = g_bPrintfOn;

        g_bRenderWindowActive = true;

        while(msg.message != WM_QUIT)
        {
            if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
				SwitchToThread();

                // if we've just switched back to display off, clear buffer & display prompt
                if(!g_bPrintfOn && lPrintfOn)
                {
                    dbgConsole->Reset();
                }

                lPrintfOn = g_bPrintfOn;

                dbgConsole->Process();
            }
        }

        g_bRenderWindowActive = false;

        delete dbgConsole;

        CxbxKrnlCleanup(nullptr);
    }

	DbgPrintf("D3D8: Message-Pump thread is finished\n");

    return 0;
}

// simple helper function
void ToggleFauxFullscreen(HWND hWnd)
{
    if(g_XBVideo.GetFullscreen())
        return;

    static LONG lRestore = 0, lRestoreEx = 0;
    static RECT lRect = {0};

    if(!g_bIsFauxFullscreen)
    {
        if(CxbxKrnl_hEmuParent != NULL)
        {
            SetParent(hWnd, NULL);
        }
        else
        {
            lRestore = GetWindowLong(hWnd, GWL_STYLE);
            lRestoreEx = GetWindowLong(hWnd, GWL_EXSTYLE);

            GetWindowRect(hWnd, &lRect);
        }

        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);
        ShowWindow(hWnd, SW_MAXIMIZE);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
    else
    {
        if(CxbxKrnl_hEmuParent != NULL)
        {
            SetParent(hWnd, CxbxKrnl_hEmuParent);
            SetWindowLong(hWnd, GWL_STYLE, WS_CHILD);
            ShowWindow(hWnd, SW_MAXIMIZE);
            SetFocus(CxbxKrnl_hEmuParent);
        }
        else
        {
            SetWindowLong(hWnd, GWL_STYLE, lRestore);
            SetWindowLong(hWnd, GWL_EXSTYLE, lRestoreEx);
            ShowWindow(hWnd, SW_RESTORE);
            SetWindowPos(hWnd, HWND_NOTOPMOST, lRect.left, lRect.top, lRect.right - lRect.left, lRect.bottom - lRect.top, 0);
            SetFocus(hWnd);
        }
    }

    g_bIsFauxFullscreen = !g_bIsFauxFullscreen;
}

// rendering window message procedure
static LRESULT WINAPI EmuMsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool bAutoPaused = false;

    switch(msg)
    {
        case WM_DESTROY: // Either SendMessage(WM_CLOSE, or close cross was pressed
        {
            DeleteObject(g_hBgBrush);
            PostQuitMessage(0);
            return D3D_OK; // = 0
        }
        break;

        case WM_SYSKEYDOWN: // = ALT + ??
        {
            if(wParam == VK_RETURN)
            {
                ToggleFauxFullscreen(hWnd);
            }
            else if(wParam == VK_F4)
            {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }
        }
        break;

        case WM_KEYDOWN:
        {
            /*! disable fullscreen if we are set to faux mode, and faux fullscreen is active */
            if(wParam == VK_ESCAPE)
            {
                if(g_XBVideo.GetFullscreen())
                {
                    SendMessage(hWnd, WM_CLOSE, 0, 0);
                }
                else if(g_bIsFauxFullscreen)
                {
                    ToggleFauxFullscreen(hWnd);
                }
            }
			else if (wParam == VK_F2)
			{
				g_bIntegrityChecking = !g_bIntegrityChecking;
			}
			else if (wParam == VK_F6)
			{
				// For some unknown reason, F6 isn't handled in WndMain::WndProc
				// sometimes, so detect it and stop emulation from here too :
				SendMessage(hWnd, WM_CLOSE, 0, 0); // See StopEmulation();
			}
			else if(wParam == VK_F8)
            {
                g_bPrintfOn = !g_bPrintfOn;
            }
            else if(wParam == VK_F9)
            {
                XTL::g_bBrkPush = TRUE;
            }
            else if(wParam == VK_F10)
            {
                ToggleFauxFullscreen(hWnd);
            }
            else if(wParam == VK_F11)
            {
                if(++g_FillModeOverride > 3)
                    g_FillModeOverride = 0;
            }
            else if(wParam == VK_F12)
            {
                XTL::g_bStepPush = !XTL::g_bStepPush;
            }
        }
        break;

        case WM_SIZE:
        {
            switch(wParam)
            {
                case SIZE_RESTORED:
                case SIZE_MAXIMIZED:
                {
                    if(bAutoPaused)
                    {
                        bAutoPaused = false;
                        CxbxKrnlResume();
                    }
                }
                break;

                case SIZE_MINIMIZED:
                {
                    if(g_XBVideo.GetFullscreen())
                        CxbxKrnlCleanup(nullptr);

                    if(!g_bEmuSuspended)
                    {
                        bAutoPaused = true;
                        CxbxKrnlSuspend();
                    }
                }
                break;
            }
        }
        break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_SETFOCUS:
        {
            if(CxbxKrnl_hEmuParent != NULL)
            {
                SetFocus(CxbxKrnl_hEmuParent);
            }
        }
        break;

        case WM_SETCURSOR:
        {
            if(g_XBVideo.GetFullscreen() || g_bIsFauxFullscreen)
            {
                SetCursor(NULL);
                return D3D_OK; // = 0
            }

            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        break;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return D3D_OK; // = 0
}

std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double, std::nano>> GetNextVBlankTime()
{
	using namespace std::literals::chrono_literals;

	// TODO: Read display frequency from Xbox Display Adapter
	// This is accessed by calling CMiniport::GetRefreshRate(); 
	// This reads from the structure located at CMiniPort::m_CurrentAvInfo
	// This will require at least Direct3D_CreateDevice being unpatched
	// otherwise, m_CurrentAvInfo will never be initialised!
	// 20ms should be used in the case of 50hz
	return std::chrono::steady_clock::now() + 16.6666666667ms;
}


// timing thread procedure
static DWORD WINAPI EmuUpdateTickCount(LPVOID)
{
	DbgPrintf("D3D8: Timing thread started\n");

    // since callbacks come from here
	InitXboxThread(g_CPUOthers); // avoid Xbox1 core for lowest possible latency

    // current vertical blank count
    int curvb = 0;

	// Calculate Next VBlank time
	auto nextVBlankTime = GetNextVBlankTime();
    DbgPrintf("D3D8: Timing thread is running\n");

    while(true)
    {
        xboxkrnl::KeTickCount = timeGetTime();	
		SwitchToThread();

        //
        // Poll input
        //

        {
            int v;

            for(v=0;v<XINPUT_SETSTATE_SLOTS;v++)
            {
                HANDLE hDevice = g_pXInputSetStateStatus[v].hDevice;

                if(hDevice == 0)
                    continue;

                DWORD dwLatency = g_pXInputSetStateStatus[v].dwLatency++;

                if(dwLatency < XINPUT_SETSTATE_LATENCY)
                    continue;

                g_pXInputSetStateStatus[v].dwLatency = 0;

                XTL::PXINPUT_FEEDBACK pFeedback = (XTL::PXINPUT_FEEDBACK)g_pXInputSetStateStatus[v].pFeedback;

                if(pFeedback == 0)
                    continue;

                //
                // Only update slot if it has not already been updated
                //

                if(pFeedback->Header.dwStatus != ERROR_SUCCESS)
                {
                    if(pFeedback->Header.hEvent != 0)
                    {
                        SetEvent(pFeedback->Header.hEvent);
                    }

                    pFeedback->Header.dwStatus = ERROR_SUCCESS;
                }
            }
        }

		// If VBlank Interval has passed, trigger VBlank callback
        // Note: This whole code block can be removed once NV2A interrupts are implemented
		// And Both Swap and Present can be ran unpatched
		// Once that is in place, MiniPort + Direct3D will handle this on it's own!
		// We check for LLE flag as NV2A handles it's own VBLANK if LLE is enabled!
		if (!(bLLE_GPU) && std::chrono::steady_clock::now() > nextVBlankTime)
        {
			nextVBlankTime = GetNextVBlankTime();

			// Increment the VBlank Counter and Wake all threads there were waiting for the VBlank to occur
			std::unique_lock<std::mutex> lk(g_VBConditionMutex);
            g_VBData.VBlankCounter++;
			g_VBConditionVariable.notify_all();

			// TODO: Fixme.  This may not be right...
			g_SwapData.SwapVBlank = 1;

            if(g_pVBCallback != NULL)
            {
                    
                g_pVBCallback(&g_VBData);
                    
            }

            g_VBData.SwapCounter = 0;

			// TODO: This can't be accurate...
			g_SwapData.TimeUntilSwapVBlank = 0;

			// TODO: Recalculate this for PAL version if necessary.
			// Also, we should check the D3DPRESENT_INTERVAL value for accurracy.
		//	g_SwapData.TimeBetweenSwapVBlanks = 1/60;
			g_SwapData.TimeBetweenSwapVBlanks = 0;
        }
    }

	DbgPrintf("D3D8: Timing thread is finished\n");
}

void CxbxReleaseSurface(XTL::IDirect3DSurface8 **ppSurface)
{
	if (*ppSurface != nullptr)
	{
		(*ppSurface)->UnlockRect(); // remove old lock
		(*ppSurface)->Release();
		(*ppSurface) = nullptr;
	}
}

void CxbxGetActiveHostBackBuffer()
{
	if (g_pActiveHostBackBuffer == nullptr)
	{
		LOG_INIT // Allows use of DEBUG_D3DRESULT

		// Refresh the host backbuffer 
		HRESULT hRet = g_pD3DDevice8->GetBackBuffer(0, XTL::D3DBACKBUFFER_TYPE_MONO, &g_pActiveHostBackBuffer);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetBackBuffer");

		if (FAILED(hRet))
			CxbxKrnlCleanup("Unable to retrieve back buffer");

		assert(g_pActiveHostBackBuffer != nullptr);
	}
}

void CxbxClear
(
	DWORD                Count,
	CONST XTL::D3DRECT  *pRects,
	DWORD                Flags,
	XTL::D3DCOLOR        Color,
	float                Z,
	DWORD                Stencil
)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	// make adjustments to parameters to make sense with windows d3d
	DWORD PCFlags = XTL::EmuXB2PC_D3DCLEAR_FLAGS(Flags);
	{
		if (Flags & XTL::X_D3DCLEAR_TARGET) {
			// TODO: D3DCLEAR_TARGET_A, *R, *G, *B don't exist on windows
			if ((Flags & XTL::X_D3DCLEAR_TARGET) != XTL::X_D3DCLEAR_TARGET)
				EmuWarning("Unsupported : Partial D3DCLEAR_TARGET flag(s) for D3DDevice_Clear : 0x%.08X", Flags & XTL::X_D3DCLEAR_TARGET);
		}

		if (Flags & (XTL::X_D3DCLEAR_ZBUFFER | XTL::X_D3DCLEAR_ZBUFFER)) {
			// Only when Z/Depth flags are given, check if these components are present :
			UpdateDepthStencilFlags(GetXboxDepthStencil()); // Update g_bHasDepthBits and g_bHasStencilBits

			// Do not needlessly clear Z Buffer
			if (Flags & XTL::X_D3DCLEAR_ZBUFFER) {
				if (!g_bHasDepthBits) {
					PCFlags &= ~D3DCLEAR_ZBUFFER;
					EmuWarning("Ignored D3DCLEAR_ZBUFFER flag (there's no Depth component in the DepthStencilSurface)");
				}
			}

			// Only clear depth buffer and stencil if present
			//
			// Avoids following DirectX Debug Runtime error report
			//    [424] Direct3D8: (ERROR) :Invalid flag D3DCLEAR_ZBUFFER: no zbuffer is associated with device. Clear failed. 
			if (Flags & XTL::X_D3DCLEAR_STENCIL) {
				if (!g_bHasStencilBits) {
					PCFlags &= ~D3DCLEAR_STENCIL;
					EmuWarning("Ignored D3DCLEAR_STENCIL flag (there's no Stencil component in the DepthStencilSurface)");
				}
			}
		}
		if (Flags & ~(XTL::X_D3DCLEAR_TARGET | XTL::X_D3DCLEAR_ZBUFFER | XTL::X_D3DCLEAR_STENCIL))
			EmuWarning("Unsupported Flag(s) for D3DDevice_Clear : 0x%.08X", Flags & ~(XTL::X_D3DCLEAR_TARGET | XTL::X_D3DCLEAR_ZBUFFER | XTL::X_D3DCLEAR_STENCIL));
	}

	// Since we filter the flags, make sure there are some left (else, clear isn't necessary) :
	if (PCFlags > 0)
	{
		HRESULT hRet;

		// Before clearing, make sure the correct output surfaces are used
		CxbxUpdateActiveRenderTarget(); // No need to call full-fledged CxbxUpdateNativeD3DResources

		hRet = g_pD3DDevice8->Clear(Count, pRects, PCFlags, Color, Z, Stencil);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->Clear");
	}
}

HRESULT CxbxReset()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	// https ://msdn.microsoft.com/en-us/library/windows/desktop/bb174425(v=vs.85).aspx
	// "Before calling the IDirect3DDevice9::Reset method for a device, an application should
	// release any explicit render targets, depth stencil surfaces, additional swap chains,
	// state blocks, and D3DPOOL_DEFAULT resources associated with the device." :
	CxbxClearD3DDeviceState();

	HRESULT hRet = g_pD3DDevice8->Reset(&(g_EmuCDPD.HostPresentationParameters));
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->Reset");

	return hRet;
}

// A wrapper for Present() with an extra safeguard to restore 'device lost' errors
void CxbxPresent()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	HRESULT hRet;

	CxbxReleaseSurface(&g_pActiveHostBackBuffer);

	hRet = g_pD3DDevice8->EndScene();
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->EndScene");

	hRet = g_pD3DDevice8->Present(nullptr, nullptr, 0, nullptr);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->Present");

	if (hRet == D3DERR_DEVICELOST) {
		while (hRet != D3D_OK)
		{
			hRet = g_pD3DDevice8->TestCooperativeLevel();
			if (hRet == D3DERR_DEVICELOST) // Device is lost and cannot be reset yet
				Sleep(500); // Wait a bit so we don't burn through cycles for no reason
			else
				if (hRet == D3DERR_DEVICENOTRESET) // Lost but we can reset it now
					hRet = CxbxReset();
		}

		CxbxInitD3DState();
	}

	hRet = g_pD3DDevice8->BeginScene();
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->BeginScene");

	CxbxGetActiveHostBackBuffer();
}

// thread dedicated to create devices
static DWORD WINAPI EmuCreateDeviceProxy(LPVOID) // TODO : Figure out whether it's still needed to do this via a proxy thread
{
	LOG_FUNC(); // Required for DEBUG_D3DRESULT (_logFuncPrefix)

    DbgPrintf("D3D8: CreateDevice proxy thread is running\n");

    while (true) {
        // if we have been signalled, create the device with cached parameters
        if (g_EmuCDPD.bSignalled) {
            DbgPrintf("D3D8: CreateDevice proxy thread received request.\n");
			if (g_EmuCDPD.bCreate) {
				// only one device should be created at once
				// TODO: ensure all surfaces are somehow cleaned up?
				if (g_pD3DDevice8 == nullptr) {
					DbgPrintf("D3D8: CreateDevice proxy thread creating new Device.\n");
				}
				else{
					DbgPrintf("D3D8: CreateDevice proxy thread re-creating Device (releasing old first).\n");
					g_pD3DDevice8->EndScene();

					// Address DirectX Debug Runtime reported error in _DEBUG builds
					// Direct3D8: (ERROR) :Not all objects were freed: the following indicate the types of unfreed objects.
					CxbxClearD3DDeviceState();

					// Cleanup previous device (and all related state) :
					while (g_pD3DDevice8->Release() > 0)
						;

					// Prevent exceptions on stale pointers
					CxbxClearGlobals();
				}

				if (g_EmuCDPD.XboxPresentationParameters.BufferSurfaces[0] != NULL)
					EmuWarning("BufferSurfaces[0] : 0x%.08X", g_EmuCDPD.XboxPresentationParameters.BufferSurfaces[0]);

				if (g_EmuCDPD.XboxPresentationParameters.DepthStencilSurface != NULL)
					EmuWarning("DepthStencilSurface : 0x%.08X", g_EmuCDPD.XboxPresentationParameters.DepthStencilSurface);

				// make adjustments to parameters to make sense with windows Direct3D
				{
					// Make sure we're working with an empty structure
					g_EmuCDPD.HostPresentationParameters = {};

					g_EmuCDPD.HostPresentationParameters.BackBufferWidth = g_EmuCDPD.XboxPresentationParameters.BackBufferWidth;
					g_EmuCDPD.HostPresentationParameters.BackBufferHeight = g_EmuCDPD.XboxPresentationParameters.BackBufferHeight;
					g_EmuCDPD.HostPresentationParameters.BackBufferFormat = EmuXB2PC_D3DFormat(g_EmuCDPD.XboxPresentationParameters.BackBufferFormat);
					g_EmuCDPD.HostPresentationParameters.FullScreen_RefreshRateInHz = g_EmuCDPD.XboxPresentationParameters.FullScreen_RefreshRateInHz;
					g_EmuCDPD.HostPresentationParameters.Windowed = !g_XBVideo.GetFullscreen();
					g_EmuCDPD.HostPresentationParameters.SwapEffect = (XTL::D3DSWAPEFFECT)g_EmuCDPD.XboxPresentationParameters.SwapEffect;

					if (g_EmuCDPD.HostPresentationParameters.BackBufferFormat == XTL::D3DFMT_A8R8G8B8) {
						EmuWarning("Changing host BackBufferFormat from D3DFMT_A8R8G8B8 to D3DFMT_X8R8G8B8, to make CheckDeviceFormat() succeed more often");
						g_EmuCDPD.HostPresentationParameters.BackBufferFormat = XTL::D3DFMT_X8R8G8B8;
					}

					// MultiSampleType may only be set if SwapEffect = D3DSWAPEFFECT_DISCARD :
					if (g_EmuCDPD.HostPresentationParameters.SwapEffect == XTL::D3DSWAPEFFECT_DISCARD)
					{
						// TODO: Support Xbox extensions if possible
						// if(g_EmuCDPD.XboxPresentationParameters.MultiSampleType != 0) {
						//	EmuWarning("MultiSampleType 0x%.08X is not supported!", g_EmuCDPD.XboxPresentationParameters.MultiSampleType);

						//g_EmuCDPD.HostPresentationParameters.MultiSampleType = EmuXB2PC_D3DMULTISAMPLE_TYPE(g_EmuCDPD.XboxPresentationParameters.MultiSampleType);

						// TODO: Check card for multisampling abilities
			//            if(XboxPresentationParameters.MultiSampleType == X_D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_QUINCUNX) // = 0x00001121
			//                XboxPresentationParameters.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
			//            else
			//                CxbxKrnlCleanup("Unknown MultiSampleType (0x%.08X)", XboxPresentationParameters.MultiSampleType);
					}
					//else
					g_EmuCDPD.HostPresentationParameters.MultiSampleType = XTL::D3DMULTISAMPLE_NONE;

					// Set BackBufferCount (if this is too much, CreateDevice will change this into the allowed maximum,
					// so a second call should succeed) :
					g_EmuCDPD.HostPresentationParameters.BackBufferCount = g_EmuCDPD.XboxPresentationParameters.BackBufferCount;
					if (g_EmuCDPD.HostPresentationParameters.SwapEffect == XTL::D3DSWAPEFFECT_COPY)
					{
						EmuWarning("Limiting BackBufferCount to 1 because of D3DSWAPEFFECT_COPY...");
						// HACK: Disable Tripple Buffering for now...
						// TODO: Enumerate maximum BackBufferCount if possible.
						g_EmuCDPD.HostPresentationParameters.BackBufferCount = 1;
					}

					g_EmuCDPD.HostPresentationParameters.hDeviceWindow = g_EmuCDPD.XboxPresentationParameters.hDeviceWindow;
					g_EmuCDPD.HostPresentationParameters.EnableAutoDepthStencil = g_EmuCDPD.XboxPresentationParameters.EnableAutoDepthStencil;
					g_EmuCDPD.HostPresentationParameters.AutoDepthStencilFormat = EmuXB2PC_D3DFormat(g_EmuCDPD.XboxPresentationParameters.AutoDepthStencilFormat);
					g_EmuCDPD.HostPresentationParameters.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

					DWORD PresentationInterval;

					if (!g_XBVideo.GetVSync() && (g_D3DCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) && g_XBVideo.GetFullscreen())
						PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
					else
					{
						if (g_D3DCaps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE && g_XBVideo.GetFullscreen())
							PresentationInterval = D3DPRESENT_INTERVAL_ONE;
						else
							PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
					}

#if DXBX_USE_D3D9
					g_EmuCDPD.HostPresentationParameters.PresentationInterval = PresentationInterval;
#else
					g_EmuCDPD.HostPresentationParameters.FullScreen_PresentationInterval = PresentationInterval;
#endif
				}

				g_EmuCDPD.HostCreationParameters.AdapterOrdinal = g_XBVideo.GetDisplayAdapter();
				g_EmuCDPD.HostCreationParameters.DeviceType = g_D3DDeviceType;
				// Note: Instead of the hFocusWindow argument, we use the global g_hEmuWindow here:
				g_EmuCDPD.HostCreationParameters.hFocusWindow = g_hEmuWindow;
				g_EmuCDPD.HostCreationParameters.BehaviorFlags = 0;

				// detect vertex processing capabilities
				if ((g_D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) && g_EmuCDPD.HostCreationParameters.DeviceType == XTL::D3DDEVTYPE_HAL)
				{
					DbgPrintf("D3D8: Using hardware vertex processing\n");

					g_EmuCDPD.HostCreationParameters.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
					g_dwVertexShaderUsage = 0;
				}
				else
				{
					DbgPrintf("D3D8: Using software vertex processing\n");

					g_EmuCDPD.HostCreationParameters.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
					g_dwVertexShaderUsage = D3DUSAGE_SOFTWAREPROCESSING;
				}

				// Dxbx addition : Prevent Direct3D from changing the FPU Control word :
				g_EmuCDPD.HostCreationParameters.BehaviorFlags |= D3DCREATE_FPU_PRESERVE;

				// Address debug DirectX runtime warning in _DEBUG builds
				// Direct3D8: (WARN) :Device that was created without D3DCREATE_MULTITHREADED is being used by a thread other than the creation thread.
				g_EmuCDPD.HostCreationParameters.BehaviorFlags |= D3DCREATE_MULTITHREADED;

				// redirect to windows Direct3D
				g_EmuCDPD.hRet = g_pD3D8->CreateDevice
				(
					g_EmuCDPD.HostCreationParameters.AdapterOrdinal,
					g_EmuCDPD.HostCreationParameters.DeviceType,
					g_EmuCDPD.HostCreationParameters.hFocusWindow,
					g_EmuCDPD.HostCreationParameters.BehaviorFlags,
					&(g_EmuCDPD.HostPresentationParameters),
					&g_pD3DDevice8
				);
				DEBUG_D3DRESULT(g_EmuCDPD.hRet, "IDirect3D8::CreateDevice");

				// See if the BackBufferCount was too large - retry if it was updated :
				if ((FAILED(g_EmuCDPD.hRet))
					&& (g_EmuCDPD.HostPresentationParameters.SwapEffect != XTL::D3DSWAPEFFECT_COPY)
					&& (g_EmuCDPD.HostPresentationParameters.BackBufferCount != g_EmuCDPD.XboxPresentationParameters.BackBufferCount))
				{
					EmuWarning("BackBufferCount too large! D3D changed it to %d, so retrying...", g_EmuCDPD.HostPresentationParameters.BackBufferCount);
					g_EmuCDPD.hRet = g_pD3D8->CreateDevice
					(
						g_EmuCDPD.HostCreationParameters.AdapterOrdinal,
						g_EmuCDPD.HostCreationParameters.DeviceType,
						g_EmuCDPD.HostCreationParameters.hFocusWindow,
						g_EmuCDPD.HostCreationParameters.BehaviorFlags,
						&(g_EmuCDPD.HostPresentationParameters),
						&g_pD3DDevice8
					);
					DEBUG_D3DRESULT(g_EmuCDPD.hRet, "IDirect3D8::CreateDevice");
				}

				if (FAILED(g_EmuCDPD.hRet))
                    CxbxKrnlCleanup("IDirect3D8::CreateDevice failed");

				HRESULT hRet;

				hRet = g_pD3DDevice8->BeginScene();
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->BeginScene");

				// Update XboxPresentationParameters from actual HostPresentationParameters
				g_EmuCDPD.XboxPresentationParameters.BackBufferWidth = g_EmuCDPD.HostPresentationParameters.BackBufferWidth;
				g_EmuCDPD.XboxPresentationParameters.BackBufferHeight = g_EmuCDPD.HostPresentationParameters.BackBufferHeight;
				g_EmuCDPD.XboxPresentationParameters.BackBufferFormat = EmuPC2XB_D3DFormat(g_EmuCDPD.HostPresentationParameters.BackBufferFormat);
				g_EmuCDPD.XboxPresentationParameters.BackBufferCount = g_EmuCDPD.HostPresentationParameters.BackBufferCount;

				// default NULL guid
				g_ddguid = { 0 };

                // enumerate device guid for this monitor, for directdraw
				hRet = XTL::DirectDrawEnumerateExA(EmuEnumDisplayDevices, nullptr, DDENUM_ATTACHEDSECONDARYDEVICES);
				DEBUG_D3DRESULT(hRet, "DirectDrawEnumerateExA");

                // create DirectDraw7
                {
                    if(FAILED(hRet)) {
                        hRet = XTL::DirectDrawCreateEx(nullptr, (void**)&g_pDD7, XTL::IID_IDirectDraw7, nullptr);
						DEBUG_D3DRESULT(hRet, "XTL::DirectDrawCreateEx(NULL)");
					} else {
						hRet = XTL::DirectDrawCreateEx(&g_ddguid, (void**)&g_pDD7, XTL::IID_IDirectDraw7, nullptr);
						DEBUG_D3DRESULT(hRet, "XTL::DirectDrawCreateEx(&g_ddguid)");
					}

					if(FAILED(hRet))
                        CxbxKrnlCleanup("Could not initialize DirectDraw7");

					g_DriverCaps = {};
					g_DriverCaps.dwSize = sizeof(XTL::DDCAPS);
					hRet = g_pDD7->GetCaps(&g_DriverCaps, nullptr);
					DEBUG_D3DRESULT(hRet, "g_pDD7->GetCaps");

                    hRet = g_pDD7->SetCooperativeLevel(NULL, DDSCL_NORMAL);
					DEBUG_D3DRESULT(hRet, "g_pDD7->SetCooperativeLevel");

                    if(FAILED(hRet))
                        CxbxKrnlCleanup("Could not set cooperative level");
                }

				g_bSupportsYUY2Overlay = IsResourceFormatSupported(XTL::X_D3DRTYPE_SURFACE, XTL::D3DFMT_YUY2, 0);

				// Dump all supported DirectDraw FourCC format codes
				{
                    DWORD  dwCodes = 0;
                    DWORD *lpCodes = nullptr;

                    g_pDD7->GetFourCCCodes(&dwCodes, lpCodes);
                    lpCodes = (DWORD*)malloc(dwCodes*sizeof(DWORD));
                    g_pDD7->GetFourCCCodes(&dwCodes, lpCodes);
                    for(DWORD v=0;v<dwCodes;v++)
                    {
						DbgPrintf("D3D8: FourCC[%d] = %.4s\n", v, (char *)&(lpCodes[v]));
						// Map known FourCC codes to Xbox Format
						int X_Format;
						switch (lpCodes[v]) {
						case MAKEFOURCC('Y', 'U', 'Y', '2'):
							X_Format = XTL::X_D3DFMT_YUY2;
							break;
						case MAKEFOURCC('U', 'Y', 'V', 'Y'):
							X_Format = XTL::X_D3DFMT_UYVY;
							break;
						case MAKEFOURCC('D', 'X', 'T', '1'):
							X_Format = XTL::X_D3DFMT_DXT1;
							break;
						case MAKEFOURCC('D', 'X', 'T', '3'):
							X_Format = XTL::X_D3DFMT_DXT3;
							break;
						case MAKEFOURCC('D', 'X', 'T', '5'):
							X_Format = XTL::X_D3DFMT_DXT5;
							break;
						default:
							continue;
						}

						// Warn if CheckDeviceFormat didn't report this format
						if (!g_bSupportsYUY2Overlay && (X_Format == XTL::X_D3DFMT_YUY2)) {
							EmuWarning("FourCC format %.4s not previously detected via CheckDeviceFormat()! Enabling it.", (char *)&(lpCodes[v]));
							// TODO : If this warning never shows, detecting FourCC's could be removed entirely. For now, enable the format :
							g_bSupportsYUY2Overlay = true;
						}
                    }

                    free(lpCodes);						
				}

				// check for YUY2 overlay support TODO: accept other overlay types
				{
                    if(!g_bSupportsYUY2Overlay)
                        EmuWarning("YUY2 overlays are not supported in hardware, could be slow!");
					else
					{
						// Does the user want to use Hardware accelerated YUV surfaces?
						if (g_XBVideo.GetHardwareYUV())
							DbgPrintf("D3D8: Hardware accelerated YUV surfaces Enabled...\n");
						else
						{
							g_bSupportsYUY2Overlay = false;
							DbgPrintf("D3D8: Hardware accelerated YUV surfaces Disabled...\n");
						}
					}
                }

                // initialize primary surface
                if(g_bSupportsYUY2Overlay)
                {
                    XTL::DDSURFACEDESC2 ddsd2 = { 0 };
					HRESULT hRet;

                    ddsd2.dwSize = sizeof(ddsd2);
                    ddsd2.dwFlags = DDSD_CAPS;
                    ddsd2.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

                    hRet = g_pDD7->CreateSurface(&ddsd2, &g_pDDSPrimary7, nullptr);
					DEBUG_D3DRESULT(hRet, "g_pDD7->CreateSurface");

					if (FAILED(hRet))
					{
						CxbxKrnlCleanup("Could not create primary surface (0x%.08X)", hRet);
						// TODO : Make up our mind: Either halt (above) or continue (below)
						g_bSupportsYUY2Overlay = false;
					}
                }

				CxbxInitD3DState();

				// initially, show a black screen
				UpdateDepthStencilFlags(GetXboxDepthStencil()); // Update g_bHasDepthBits and g_bHasStencilBits
				CxbxClear(
					/*Count=*/0,
					/*pRects=*/nullptr,
					XTL::X_D3DCLEAR_TARGET | (g_bHasDepthBits ? XTL::X_D3DCLEAR_ZBUFFER : 0) | (g_bHasStencilBits ? XTL::X_D3DCLEAR_STENCIL : 0),
					/*Color=*/0xFF000000, // TODO : Use constant for this
					/*Z=*/g_bHasDepthBits ? 1.0f : 0.0f,
					/*Stencil=*/g_bHasStencilBits ? 0 : 0); // TODO : What to set these to?
				CxbxPresent();
            }
            else { // !bCreate
                // release direct3d
				if (g_pD3DDevice8 == nullptr) {
					DbgPrintf("D3D8: CreateDevice proxy thread release without Device.\n");
				}
				else {
					DbgPrintf("D3D8: CreateDevice proxy thread releasing old Device.\n");
                    g_pD3DDevice8->EndScene();
                    g_EmuCDPD.hRet = g_pD3DDevice8->Release();
                    if(g_EmuCDPD.hRet == 0)
                        g_pD3DDevice8 = nullptr;
                }

				// cleanup overlay clipper
				if (g_pDDClipper != nullptr) {
					g_pDDClipper->Release();
					g_pDDClipper = nullptr;
				}

				// cleanup overlay surface
				if (g_pDDSOverlay7 != nullptr) {
					g_pDDSOverlay7->Release();
					g_pDDSOverlay7 = nullptr;
				}

				// cleanup directdraw surface
                if (g_pDDSPrimary7 != nullptr) {
                    g_pDDSPrimary7->Release();
                    g_pDDSPrimary7 = nullptr;
                }

                // cleanup directdraw
                if (g_pDD7 != nullptr) {
                    g_pDD7->Release();
                    g_pDD7 = nullptr;
                }
            }

            // signal completion
            g_EmuCDPD.bSignalled = false;
        }

		Sleep(250); // react within a quarter of a second to a CreateDevice signal
    }

	DbgPrintf("D3D8: CreateDevice proxy thread is finished\n");

    return 0;
}

#if 0
// ensure a given width/height are powers of 2
static void EmuAdjustPower2(UINT *dwWidth, UINT *dwHeight)
{
    UINT NewWidth=0, NewHeight=0;

    int v;

    for(v=0;v<32;v++)
    {
        int mask = 1 << v;

        if(*dwWidth & mask)
            NewWidth = mask;

        if(*dwHeight & mask)
            NewHeight = mask;
    }

    if(*dwWidth != NewWidth)
    {
        NewWidth <<= 1;
        EmuWarning("Needed to resize width (%d->%d)", *dwWidth, NewWidth);
    }

    if(*dwHeight != NewHeight)
    {
        NewHeight <<= 1;
        EmuWarning("Needed to resize height (%d->%d)", *dwHeight, NewHeight);
    }

    *dwWidth = NewWidth;
    *dwHeight = NewHeight;
}
#endif

struct ConvertedIndexBuffer {
	DWORD Hash = 0;
	DWORD uiIndexCount = 0;
	XTL::IDirect3DIndexBuffer8 *pConvertedHostIndexBuffer = nullptr;
};

XTL::IDirect3DIndexBuffer8 *CxbxUpdateIndexBuffer
(
	PWORD         pIndexBufferData,
	UINT          uiIndexCount
)
{
	static std::map<PWORD, ConvertedIndexBuffer> g_ConvertedIndexBuffers;

	LOG_INIT // Allows use of DEBUG_D3DRESULT

	if (pIndexBufferData == NULL)
		return nullptr;

	if (uiIndexCount == 0)
		return nullptr;

	uint uiIndexBufferSize = sizeof(XTL::INDEX16) * uiIndexCount;

	// TODO : Don't hash every time (peek at how the vertex buffer cache avoids this)
	uint32_t uiHash = 0; // TODO : seed with characteristics
	uiHash = XXHash32::hash((void *)pIndexBufferData, (uint64_t)uiIndexBufferSize, uiHash);

	// TODO : Lock all access to g_ConvertedIndexBuffers

	// Poor-mans cache-eviction : Clear when full.
	if (g_ConvertedIndexBuffers.size() >= MAX_CACHE_SIZE_INDEXBUFFERS) {
		DbgPrintf("Index buffer cache full - clearing and repopulating");
		for (auto it = g_ConvertedIndexBuffers.begin(); it != g_ConvertedIndexBuffers.end(); ++it) {
			auto pHostIndexBuffer = (XTL::IDirect3DIndexBuffer8 *)(it->second.pConvertedHostIndexBuffer);
			if (pHostIndexBuffer != nullptr) {
				pHostIndexBuffer->Release(); // avoid memory leaks
			}
		}

		g_ConvertedIndexBuffers.clear();
	}

	// Reference the converted index buffer (when it's not present, it's added) 
	ConvertedIndexBuffer& convertedIndexBuffer = g_ConvertedIndexBuffers[pIndexBufferData];

	// Check if the data needs an updated conversion or not
	XTL::IDirect3DIndexBuffer8 *result = convertedIndexBuffer.pConvertedHostIndexBuffer;
	if (result != nullptr)
	{
		// Only re-use if the size hasn't changed (we can't use larger buffers,
		// since those will have have different hashes from smaller buffers)
		if (uiIndexCount == convertedIndexBuffer.uiIndexCount)
		{
			if (uiHash == convertedIndexBuffer.Hash)
			{
				// Hash is still the same - assume the converted resource doesn't require updating
				// TODO : Maybe, if the converted resource gets too old, an update might still be wise
				// to cater for differences that didn't cause a hash-difference (slight chance, but still).
				return result;
			}
		}

		convertedIndexBuffer = {};
		result->Release();
		result = nullptr;
	}

	// Create a new native index buffer of the above determined size :
	HRESULT hRet = g_pD3DDevice8->CreateIndexBuffer(
		uiIndexBufferSize,
		D3DUSAGE_WRITEONLY,
		XTL::D3DFMT_INDEX16,
		XTL::D3DPOOL_MANAGED,
		&result);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateIndexBuffer");

	if (FAILED(hRet))
		CxbxKrnlCleanup("CxbxUpdateIndexBuffer: IndexBuffer Create Failed!");

	// Update the host index buffer
	BYTE* pData = nullptr;
	hRet = result->Lock(0, 0, &pData, D3DLOCK_DISCARD);
	DEBUG_D3DRESULT(hRet, "result->Lock");

	if (pData == nullptr)
		CxbxKrnlCleanup("CxbxUpdateIndexBuffer: Could not lock index buffer!");

	memcpy(pData, pIndexBufferData, uiIndexBufferSize);
	hRet = result->Unlock();
	DEBUG_D3DRESULT(hRet, "result->Unlock");

	// Update the Index Count and the hash
	convertedIndexBuffer.Hash = uiHash;
	convertedIndexBuffer.uiIndexCount = uiIndexCount;
	// Store the resource in the cache
	result->AddRef();
	convertedIndexBuffer.pConvertedHostIndexBuffer = result;

	DbgPrintf("D3D8: Copied %d indices (D3DFMT_INDEX16)\n", uiIndexCount);

	return result;
}

void CxbxUpdateActiveIndexBuffer
(
	XTL::INDEX16* pIndexBufferData,
	UINT uiIndexCount
)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	XTL::IDirect3DIndexBuffer8 *pHostIndexBuffer = CxbxUpdateIndexBuffer(pIndexBufferData, uiIndexCount);

	UINT uiIndexBase = 0;

	if (*XTL::Xbox_D3DDevice_m_IndexBase > 0) {
		// TODO : Research if (or when) using *Xbox_D3DDevice_m_IndexBase is needed
		// uiIndexBase = *Xbox_D3DDevice_m_IndexBase;
		LOG_TEST_CASE("*Xbox_D3DDevice_m_IndexBase > 0");
	}

	// Activate the new native index buffer :
	HRESULT hRet = g_pD3DDevice8->SetIndices(pHostIndexBuffer, uiIndexBase);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetIndices");

	if (FAILED(hRet))
		CxbxKrnlCleanup("CxbxUpdateActiveIndexBuffer: SetIndices Failed!");
}

void Cxbx_Direct3D_CreateDevice()
// Called by EmuD3DInit()
{
	// EMUPATCH(Direct3D_CreateDevice)(0, X_D3DDEVTYPE_HAL, 0, D3DCREATE_HARDWARE_VERTEXPROCESSING, &XboxD3DPresentParameters, &g_pD3DDevice8);
	UINT							Adapter = 0;
	XTL::X_D3DDEVTYPE				DeviceType = XTL::X_D3DDEVTYPE_HAL;
	HWND							hFocusWindow = 0;
	DWORD							BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING; // Xbox BehaviorFlags are ignored
	XTL::X_D3DPRESENT_PARAMETERS   *pPresentationParameters = &XboxD3DPresentParameters;
//	XTL::IDirect3DDevice8         **ppReturnedDeviceInterface = &g_pD3DDevice8;

	// Print a few of the pPresentationParameters contents to the console
	DbgPrintf("BackBufferWidth:        = %d\n"
			  "BackBufferHeight:       = %d\n"
			  "BackBufferFormat:       = 0x%.08X\n"
			  "BackBufferCount:        = 0x%.08X\n"
			  "SwapEffect:             = 0x%.08X\n"
			  "EnableAutoDepthStencil: = 0x%.08X\n"
			  "AutoDepthStencilFormat: = 0x%.08X\n"
			  "Flags:                  = 0x%.08X\n\n",
			  pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight,
			  pPresentationParameters->BackBufferFormat, pPresentationParameters->BackBufferCount,
			  pPresentationParameters->SwapEffect, pPresentationParameters->EnableAutoDepthStencil,
			  pPresentationParameters->AutoDepthStencilFormat, pPresentationParameters->Flags );

	// Check if this is a second call requesting identical setup to the previous one
	if (g_pD3DDevice8 != nullptr)
	if (g_EmuCDPD.XboxCreationParameters.AdapterOrdinal == Adapter)
	if (g_EmuCDPD.XboxCreationParameters.DeviceType == DeviceType)
	if (g_EmuCDPD.XboxPresentationParameters.BackBufferWidth == pPresentationParameters->BackBufferWidth)
	if (g_EmuCDPD.XboxPresentationParameters.BackBufferHeight == pPresentationParameters->BackBufferHeight)
	if (EmuXB2PC_D3DFormat(g_EmuCDPD.XboxPresentationParameters.BackBufferFormat) == EmuXB2PC_D3DFormat(pPresentationParameters->BackBufferFormat))
	if (g_EmuCDPD.XboxPresentationParameters.BackBufferCount == pPresentationParameters->BackBufferCount)
	if (g_EmuCDPD.XboxPresentationParameters.SwapEffect == pPresentationParameters->SwapEffect)
	if (g_EmuCDPD.XboxPresentationParameters.EnableAutoDepthStencil == pPresentationParameters->EnableAutoDepthStencil)
	if (g_EmuCDPD.XboxPresentationParameters.AutoDepthStencilFormat == pPresentationParameters->AutoDepthStencilFormat)
	if (g_EmuCDPD.XboxPresentationParameters.Flags == pPresentationParameters->Flags) {
		DbgPrintf("D3D8: Prevented duplicate CreateDevice proxy call.\n");
		g_EmuCDPD.XboxPresentationParameters = *pPresentationParameters; // Do use the Xbox pointer
//		*ppReturnedDeviceInterface = *g_EmuCDPD.ppReturnedDeviceInterface;
//		return g_EmuCDPD.hRet;
	}

    // Cache parameters
    g_EmuCDPD.XboxCreationParameters.AdapterOrdinal = Adapter;
    g_EmuCDPD.XboxCreationParameters.DeviceType = DeviceType;
    g_EmuCDPD.XboxCreationParameters.hFocusWindow = hFocusWindow;
    g_EmuCDPD.XboxPresentationParameters = *pPresentationParameters;
//	g_EmuCDPD.ppReturnedDeviceInterface = ppReturnedDeviceInterface;

    // Wait until proxy is done with an existing call (i highly doubt this situation will come up)
    while (g_EmuCDPD.bSignalled)
        Sleep(10);

    // Signal proxy thread, and wait for completion
    g_EmuCDPD.bCreate = true;
    g_EmuCDPD.bSignalled = true;

    // Wait until proxy is completed
    while (g_EmuCDPD.bSignalled)
        Sleep(10);

	using namespace XTL;

	// Finally, set all default render and texture states
	// All commented out states are unsupported on this version of DirectX 8
	g_pD3DDevice8->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	g_pD3DDevice8->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
	g_pD3DDevice8->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_ALPHAREF, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	g_pD3DDevice8->SetRenderState(D3DRS_DESTBLEND, D3DCMP_ALWAYS);
	g_pD3DDevice8->SetRenderState(D3DRS_ALPHAFUNC, D3DBLEND_ZERO);
	g_pD3DDevice8->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	g_pD3DDevice8->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	g_pD3DDevice8->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
		D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILREF, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILMASK, 0xffffffff);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff);
	g_pD3DDevice8->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	//g_pD3DDevice8->SetRenderState(D3DRS_BLENDCOLOR, 0);
	//g_pD3DDevice8->SetRenderState(D3DRS_SWATHWIDTH, D3DSWATH_128);
	//g_pD3DDevice8->SetRenderState(D3DRS_POLYGONOFFSETZSLOPESCALE, 0);
	//g_pD3DDevice8->SetRenderState(D3DRS_POLYGONOFFSETZOFFSET, 0);
	//g_pD3DDevice8->SetRenderState(D3DRS_POINTOFFSETENABLE, FALSE);
	//g_pD3DDevice8->SetRenderState(D3DRS_WIREFRAMEOFFSETENABLE, FALSE);
	//g_pD3DDevice8->SetRenderState(D3DRS_SOLIDOFFSETENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_FOGENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	g_pD3DDevice8->SetRenderState(D3DRS_FOGSTART, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_FOGEND, (::DWORD)(1.0f));
	g_pD3DDevice8->SetRenderState(D3DRS_FOGDENSITY, (::DWORD)(1.0f));
	g_pD3DDevice8->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_WRAP0, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_WRAP1, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_WRAP2, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_WRAP3, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_LIGHTING, TRUE);
	g_pD3DDevice8->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
	g_pD3DDevice8->SetRenderState(D3DRS_COLORVERTEX, TRUE);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKSPECULARMATERIALSOURCE, D3DMCS_COLOR2);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKDIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKAMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKEMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
	g_pD3DDevice8->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
	g_pD3DDevice8->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	g_pD3DDevice8->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	g_pD3DDevice8->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKAMBIENT, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_AMBIENT, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE, (::DWORD)(1.0f));
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MIN, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSPRITEENABLE, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALEENABLE, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_A, (::DWORD)(1.0f));
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_B, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_C, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MAX, (::DWORD)(64.0f));
	g_pD3DDevice8->SetRenderState(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE);
	g_pD3DDevice8->SetRenderState(D3DRS_PATCHSEGMENTS, (::DWORD)(1.0f));
	//g_pD3DDevice8->SetRenderState(D3DRS_PSTEXTUREMODES, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	g_pD3DDevice8->SetRenderState(D3DRS_FOGCOLOR, 0);
	g_pD3DDevice8->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	//g_pD3DDevice8->SetRenderState(D3DRS_BACKFILLMODE, D3DFILL_SOLID);
	//g_pD3DDevice8->SetRenderState(D3DRS_TWOSIDEDLIGHTING, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_ZENABLE, D3DZB_USEW);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	//g_pD3DDevice8->SetRenderState(D3DRS_FRONTFACE, D3DFRONT_CW);
	g_pD3DDevice8->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	g_pD3DDevice8->SetRenderState(D3DRS_TEXTUREFACTOR, 0xffffffff);
	g_pD3DDevice8->SetRenderState(D3DRS_ZBIAS, 0);
	//g_pD3DDevice8->SetRenderState(D3DRS_LOGICOP, D3DLOGICOP_NONE);
	g_pD3DDevice8->SetRenderState(D3DRS_EDGEANTIALIAS, FALSE);
	g_pD3DDevice8->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	g_pD3DDevice8->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xffffffff);
	//g_pD3DDevice8->SetRenderState(D3DRS_MULTISAMPLEMODERENDERTARGETS, D3DMULTISAMPLEMODE_1X);
	//g_pD3DDevice8->SetRenderState(D3DRS_SHADOWFUNC, D3DCMP_NEVER);	
	//g_pD3DDevice8->SetRenderState(D3DRS_LINEWIDTH, (DWORD)(1.0f));
	//g_pD3DDevice8->SetRenderState(D3DRS_DXT1NOISEENABLE, TRUE);
	//g_pD3DDevice8->SetRenderState(D3DRS_YUVENABLE, FALSE);
	//g_pD3DDevice8->SetRenderState(D3DRS_OCCLUSIONCULLENABLE, TRUE);
	//g_pD3DDevice8->SetRenderState(D3DRS_STENCILCULLENABLE, TRUE);
	//g_pD3DDevice8->SetRenderState(D3DRS_ROPZCMPALWAYSREAD, FALSE);
	//g_pD3DDevice8->SetRenderState(D3DRS_ROPZREAD, FALSE);
	//g_pD3DDevice8->SetRenderState(D3DRS_DONOTCULLUNCOMPRESSED, FALSE);

	// Xbox has 4 Texture Stages
	for (int stage = 0; stage < 4; stage++)	{
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ADDRESSW, D3DTADDRESS_WRAP);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MAGFILTER, D3DTEXF_POINT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MINFILTER, D3DTEXF_POINT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MIPMAPLODBIAS, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MAXMIPLEVEL, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_MAXANISOTROPY, 1);
		//g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORKEYOP, D3DTCOLORKEYOP_DISABLE);
		//g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORSIGN, 0);
		//g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE); //  D3DTOP_DISABLE caused textures to go opaque
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORARG0, D3DTA_CURRENT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_RESULTARG, D3DTA_CURRENT);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVMAT00, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVMAT01, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVMAT11, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVMAT10, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVLSCALE, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BUMPENVLOFFSET, 0);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage);
		g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_BORDERCOLOR, 0);
		//g_pD3DDevice8->SetTextureStageState(stage, D3DTSS_COLORKEYCOLOR, 0);
	}

//    return g_EmuCDPD.hRet;
}

// ******************************************************************
// * Start of all D3D patches
// ******************************************************************
BOOL WINAPI XTL::EMUPATCH(D3DDevice_IsBusy)()
{
	FUNC_EXPORTS

	LOG_FUNC();
    
    return FALSE;
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetCreationParameters)
(
		X_D3DDEVICE_CREATION_PARAMETERS *pParameters
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pParameters);

    pParameters->AdapterOrdinal = D3DADAPTER_DEFAULT;
    pParameters->DeviceType = X_D3DDEVTYPE_HAL;
    pParameters->hFocusWindow = NULL;
    pParameters->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3D_CheckDeviceFormat) // TODO : Rename to Direct3D_CheckDeviceFormat
(
    UINT                        Adapter,
    X_D3DDEVTYPE                DeviceType,
    X_D3DFORMAT                 AdapterFormat,
    DWORD                       Usage,
    X_D3DRESOURCETYPE           RType,
    X_D3DFORMAT                 CheckFormat
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Adapter)
		LOG_FUNC_ARG(DeviceType)
		LOG_FUNC_ARG(AdapterFormat)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_ARG(RType)
		LOG_FUNC_ARG(CheckFormat)
		LOG_FUNC_END;

    if(RType > 7)
        CxbxKrnlCleanup("RType > 7");

	// HACK: Return true for everything? (Hunter the Reckoning)

    HRESULT hRet = D3D_OK; /*g_pD3D8->CheckDeviceFormat
    (
        g_XBVideo.GetDisplayAdapter(), g_D3DDeviceType,
        EmuXB2PC_D3DFormat(AdapterFormat), Usage, (D3DRESOURCETYPE)RType, EmuXB2PC_D3DFormat(CheckFormat)
    );*/

    return hRet;
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetDisplayFieldStatus)
(
	X_D3DFIELD_STATUS *pFieldStatus
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pFieldStatus);

	// TODO: Read AV Flags to determine if Progressive or Interlaced
#if 1
    pFieldStatus->Field = (g_VBData.VBlankCounter%2 == 0) ? X_D3DFIELD_ODD : X_D3DFIELD_EVEN;
    pFieldStatus->VBlankCount = g_VBData.VBlankCounter;
#else
	pFieldStatus->Field = X_D3DFIELD_PROGRESSIVE;
	pFieldStatus->VBlankCount = 0;
#endif
}
#endif

// TODO: Find a test case and verify this
// At least one XDK has this as return VOID with a second input parameter.
// Is this definition incorrect, or did it change at some point?
PDWORD WINAPI XTL::EMUPATCH(D3DDevice_BeginPush)(DWORD Count)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Count);

	if (g_pPrimaryPB != nullptr)
	{
		EmuWarning("D3DDevice_BeginPush called without D3DDevice_EndPush in between?!");
		delete[] g_pPrimaryPB; // prevent a memory leak
	}

    DWORD *pRet = new DWORD[Count];

    g_dwPrimaryPBCount = Count;
    g_pPrimaryPB = pRet;

    return pRet;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_EndPush)(DWORD *pPush)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pPush);

#ifdef _DEBUG_TRACK_PB
//	DbgDumpPushBuffer(g_pPrimaryPB, g_dwPrimaryPBCount*sizeof(DWORD));
#endif

	if (g_pPrimaryPB == nullptr)
		EmuWarning("D3DDevice_EndPush called without preceding D3DDevice_BeginPush?!");
	else
	{
		CxbxUpdateNativeD3DResources();

		EmuExecutePushBufferRaw(g_pPrimaryPB);

		delete[] g_pPrimaryPB;
		g_pPrimaryPB = nullptr;
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_BeginVisibilityTest)()
{
	FUNC_EXPORTS

	LOG_FUNC();

	LOG_UNIMPLEMENTED();
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_EndVisibilityTest)
(
    DWORD                       Index
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Index);

	LOG_UNIMPLEMENTED();

    return D3D_OK;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetBackBufferScale)
(
	FLOAT x, 
	FLOAT y
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(x)
		LOG_FUNC_ARG(y)
		LOG_FUNC_END;

	LOG_IGNORED();
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetVisibilityTestResult)
(
    DWORD                       Index,
    UINT                       *pResult,
    ULONGLONG                  *pTimeStamp
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(pResult)
		LOG_FUNC_ARG(pTimeStamp)
		LOG_FUNC_END;

    // TODO: actually emulate this!?

    if(pResult != 0)
        *pResult = 640*480;

    if(pTimeStamp != 0)
        *pTimeStamp = 0;

    

    return D3D_OK;
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetDeviceCaps)
(
    X_D3DCAPS                   *pCaps
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pCaps);

    HRESULT hRet = g_pD3D8->GetDeviceCaps(g_XBVideo.GetDisplayAdapter(), g_D3DDeviceType, pCaps);
	DEBUG_D3DRESULT(hRet, "g_pD3D8->GetDeviceCaps");

	if(FAILED(hRet))
		CxbxKrnlCleanup("EmuD3DDevice_GetDeviceCaps failed!");
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_LoadVertexShader)
(
    DWORD                       Handle,
    DWORD                       Address
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(Address)
		LOG_FUNC_END;

    if (Address < D3DVS_XBOX_NR_ADDRESS_SLOTS) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		if (pHostVertexShader != nullptr) {
			for (DWORD i = Address; i < pHostVertexShader->Size; i++) {
				// TODO: This seems very fishy
				if (i < D3DVS_XBOX_NR_ADDRESS_SLOTS)
					g_VertexShaderSlots[i] = Handle;
			}
		}
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SelectVertexShader)
(
    DWORD                       Handle,
    DWORD                       Address
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(Address)
		LOG_FUNC_END;

	HRESULT hRet = D3D_OK;

    if (VshHandleIsVertexShader(Handle)) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		hRet = g_pD3DDevice8->SetVertexShader(pHostVertexShader->Handle);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader(VshHandleIsVertexShader)");
    }
    else if (Handle == NULL) {
		hRet = g_pD3DDevice8->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX0);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX0)");
	}
    else if (Address < D3DVS_XBOX_NR_ADDRESS_SLOTS) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(g_VertexShaderSlots[Address]);
        if (pHostVertexShader != nullptr) {
			hRet = g_pD3DDevice8->SetVertexShader(pHostVertexShader->Handle);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader(pHostVertexShader)");
		}
        else {
            EmuWarning("g_VertexShaderSlots[%d] = 0", Address);
			hRet = D3D_OK;
		}
    }

	if (FAILED(hRet)) {
		EmuWarning("We're lying about setting a vertext shader!");
		hRet = D3D_OK;
	}
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3D_KickOffAndWaitForIdle)() // TODO : Rename to KickOffAndWaitForIdle
{
//	FUNC_EXPORTS

	LOG_FUNC();

    // TODO: Actually do something here?

	LOG_UNIMPLEMENTED();
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3D_KickOffAndWaitForIdle2)(DWORD dwDummy1, DWORD dwDummy2)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(dwDummy1)
		LOG_FUNC_ARG(dwDummy2)
		LOG_FUNC_END;

    // TODO: Actually do something here?

	LOG_UNIMPLEMENTED();
}
#endif

#if 0 // Patch disabled
ULONG WINAPI XTL::EMUPATCH(D3DDevice_AddRef)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	// TODO: Make sure the Xbox reference count also gets updated
    ULONG ret = g_pD3DDevice8->AddRef();

    return ret;
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_BeginStateBlock)()
{
	FUNC_EXPORTS

	LOG_FUNC();

    ULONG ret = g_pD3DDevice8->BeginStateBlock();
	DEBUG_D3DRESULT(ret, "g_pD3DDevice8->BeginStateBlock");
}

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_BeginStateBig)()
{
//	FUNC_EXPORTS

   	LOG_FUNC();

    //ULONG hRet = g_pD3DDevice8->BeginStateBlock();
	//DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->BeginStateBlock");

	LOG_UNIMPLEMENTED();
    CxbxKrnlCleanup("BeginStateBig is not implemented");    

    return hRet;
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_CaptureStateBlock)(DWORD Token)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Token);

    ULONG ret = g_pD3DDevice8->CaptureStateBlock(Token);
	DEBUG_D3DRESULT(ret, "g_pD3DDevice8->CaptureStateBlock");
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_ApplyStateBlock)(DWORD Token)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Token);

    ULONG ret = g_pD3DDevice8->ApplyStateBlock(Token);
	DEBUG_D3DRESULT(ret, "g_pD3DDevice8->ApplyStateBlock");
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_EndStateBlock)(DWORD *pToken)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pToken);

    ULONG ret = g_pD3DDevice8->EndStateBlock(pToken);
	DEBUG_D3DRESULT(ret, "g_pD3DDevice8->EndStateBlock");

    return ret;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_CopyRects)
(
    X_D3DSurface       *pSourceSurface,
    CONST RECT         *pSourceRectsArray,
    UINT                cRects,
    X_D3DSurface       *pDestinationSurface,
    CONST POINT        *pDestPointsArray
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pSourceSurface)
		LOG_FUNC_ARG(pSourceRectsArray)
		LOG_FUNC_ARG(cRects)
		LOG_FUNC_ARG(pDestinationSurface)
		LOG_FUNC_ARG(pDestPointsArray)
		LOG_FUNC_END;

	// TODO : Do we need : CxbxUpdateActiveRenderTarget(); // Make sure the correct output surfaces are used

	XTL::IDirect3DSurface8 *pHostSourceSurface = CxbxUpdateSurface(pSourceSurface);
	XTL::IDirect3DSurface8 *pHostDestinationSurface = CxbxUpdateSurface(pDestinationSurface);

	if (pHostSourceSurface != nullptr)
		pHostSourceSurface->UnlockRect(); // remove old lock

	if (pHostDestinationSurface != nullptr)
		pHostDestinationSurface->UnlockRect(); // remove old lock

    /*
    static int kthx = 0;
    char fileName[255];

    sprintf(fileName, "C:\\Aaron\\Textures\\SourceSurface-%d.bmp", kthx++);

    D3DXSaveSurfaceToFile(fileName, D3DXIFF_BMP, pHostSourceSurface, NULL, NULL);
    //*/

    HRESULT hRet = g_pD3DDevice8->CopyRects
    (
		pHostSourceSurface,
        pSourceRectsArray,
        cRects,
		pHostDestinationSurface,
        pDestPointsArray
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CopyRects");    
}

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateImageSurface)
(
    UINT                Width,
    UINT                Height,
    X_D3DFORMAT         Format,
    X_D3DSurface      **ppSurface
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Width)
		LOG_FUNC_ARG(Height)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_ARG(ppSurface)
		LOG_FUNC_END;

    *ppSurface = EmuNewD3DSurface();
	XTL::IDirect3DSurface8 *pNewHostSurface = nullptr;

    D3DFORMAT PCFormat = EmuXB2PC_D3DFormat(Format);
    HRESULT hRet = g_pD3DDevice8->CreateImageSurface(Width, Height, PCFormat, &pNewHostSurface);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateImageSurface");

	if(FAILED(hRet))
		if(Format == X_D3DFMT_LIN_D24S8)
		{
			EmuWarning("CreateImageSurface: D3DFMT_LIN_D24S8 -> D3DFMT_A8R8G8B8");
			hRet = g_pD3DDevice8->CreateImageSurface(Width, Height, D3DFMT_A8R8G8B8, &pNewHostSurface);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateImageSurface(ARGB)");
		}
	
	if(FAILED(hRet))
		/*EmuWarning*/CxbxKrnlCleanup("CreateImageSurface failed!\nFormat = 0x%8.8X", Format);
	else
		SetHostSurface(*ppSurface, pNewHostSurface);  

    return hRet;
}
#endif

inline BYTE DownsampleWordToByte(WORD value)
{
	return (value >> 8);
}

inline WORD UpsampleByteToWord(BYTE value)
{
	// Make the error uniform instead of skewed -
	// estimate it with a value right in the centre
	// https://stackoverflow.com/a/16976296/12170
	return (value << 8) + 127;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetGammaRamp)
(
    X_D3DGAMMARAMP     *pRamp
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pRamp);

    D3DGAMMARAMP HostGammaRamp;

    g_pD3DDevice8->GetGammaRamp(&HostGammaRamp);

	for (int v = 0; v<256; v++)
	{
		// Convert host double byte to xbox single byte :
		pRamp->red[v] = DownsampleWordToByte(HostGammaRamp.red[v]);
        pRamp->green[v] = DownsampleWordToByte(HostGammaRamp.green[v]);
        pRamp->blue[v] = DownsampleWordToByte(HostGammaRamp.blue[v]);
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetGammaRamp)
(
	DWORD                   dwFlags,
	CONST X_D3DGAMMARAMP   *pRamp
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(dwFlags)
		LOG_FUNC_ARG(pRamp)
		LOG_FUNC_END;

	// remove D3DSGR_IMMEDIATE
	DWORD dwPCFlags = dwFlags & (~0x00000002);

	D3DGAMMARAMP HostGammaRamp;

	for (int v = 0; v<256; v++)
	{
		// Convert xbox single byte to host double byte :
		HostGammaRamp.red[v] = UpsampleByteToWord(pRamp->red[v]);
		HostGammaRamp.green[v] = UpsampleByteToWord(pRamp->green[v]);
		HostGammaRamp.blue[v] = UpsampleByteToWord(pRamp->blue[v]);
	}

	g_pD3DDevice8->SetGammaRamp(dwPCFlags, &HostGammaRamp);
}

#if 0 // Patch disabled
XTL::X_D3DSurface* WINAPI XTL::EMUPATCH(D3DDevice_GetBackBuffer2)
(
    INT                 BackBuffer
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(BackBuffer);

    /** unsafe, somehow
    HRESULT hRet = D3D_OK;

    X_D3DSurface *pBackBuffer = EmuNewD3DSurface();
	
    if(BackBuffer == -1)
    {
        static IDirect3DSurface8 *pCachedPrimarySurface = nullptr;

        if(pCachedPrimarySurface == nullptr)
        {
            // create a buffer to return
            // TODO: Verify the surface is always 640x480
            hRet = g_pD3DDevice8->CreateImageSurface(640, 480, D3DFMT_A8R8G8B8, &pCachedPrimarySurface);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateImageSurface");
        }

        SetHostSurface(pBackBuffer, pCachedPrimarySurface);

        hRet = g_pD3DDevice8->GetFrontBuffer(pCachedPrimarySurface);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetFrontBuffer");

		if(FAILED(hRet))
        {
            EmuWarning("Could not retrieve primary surface, using backbuffer");
			SetHostSurface(pBackBuffer, nullptr);
            pCachedPrimarySurface->Release();
            pCachedPrimarySurface = nullptr;
            BackBuffer = 0;
        }
    }

    if(BackBuffer != -1) {
        hRet = g_pD3DDevice8->GetBackBuffer(BackBuffer, D3DBACKBUFFER_TYPE_MONO, &pCachedPrimarySurface);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetBackBuffer");
	}
    //*/

	// When pBackBuffer is declared static, LensFlare XDK sample fails in SetHostSurface
	// probably because it's been freeed by the Xbox heap (while host allocates it).
	// Conclusion : We don't want this patch. We want CreateDevice running unpatched...
	X_D3DSurface *pBackBuffer = EmuNewD3DSurface();

    if(BackBuffer == -1)
        BackBuffer = 0;

	CxbxGetActiveHostBackBuffer();

	ConvertHostSurfaceHeaderToXbox(g_pActiveHostBackBuffer, pBackBuffer);

	//SetHostSurface(pBackBuffer, g_pActiveHostBackBuffer);

    // update data pointer
	XTL::D3DLOCKED_RECT LockedRect;
	g_pActiveHostBackBuffer->LockRect(&LockedRect, nullptr, 0);
	pBackBuffer->Data = (DWORD)LockedRect.pBits; // Was CXBX_D3DRESOURCE_DATA_BACK_BUFFER;

    RETURN(pBackBuffer);
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetBackBuffer)
(
    INT                 BackBuffer,
    D3DBACKBUFFER_TYPE  Type,
    X_D3DSurface      **ppBackBuffer
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_GetBackBuffer2");

    *ppBackBuffer = EMUPATCH(D3DDevice_GetBackBuffer2)(BackBuffer);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetViewport)
(
    CONST D3DVIEWPORT8 *pViewport
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pViewport);

    DWORD dwWidth  = pViewport->Width;
    DWORD dwHeight = pViewport->Height;

    // resize to fit screen (otherwise crashes occur)
    /*{
        if(dwWidth > 640)
        {
            EmuWarning("Resizing Viewport->Width to 640");
            ((D3DVIEWPORT8*)pViewport)->Width = 640;
        }

        if(dwHeight > 480)
        {
            EmuWarning("Resizing Viewport->Height to 480");
            ((D3DVIEWPORT8*)pViewport)->Height = 480;
        }
    }*/

    HRESULT hRet = g_pD3DDevice8->SetViewport(pViewport);

    // restore originals
    /*{
        if(dwWidth > 640)
            ((D3DVIEWPORT8*)pViewport)->Width = dwWidth;

        if(dwHeight > 480)
            ((D3DVIEWPORT8*)pViewport)->Height = dwHeight;
    }

	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetViewport");

	if(FAILED(hRet))
    {
        EmuWarning("Unable to set viewport! We're lying");
        hRet = D3D_OK;
    }*/
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetViewport)
(
    D3DVIEWPORT8 *pViewport
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pViewport);

    HRESULT hRet = g_pD3DDevice8->GetViewport(pViewport);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetViewport");

    if(FAILED(hRet))
    {
        EmuWarning("Unable to get viewport! - We're lying");

        hRet = D3D_OK;
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetViewportOffsetAndScale)
(
    D3DXVECTOR4 *pOffset,
    D3DXVECTOR4 *pScale
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pOffset)
		LOG_FUNC_ARG(pScale)
		LOG_FUNC_END;

    float fScaleX = 1.0f;
    float fScaleY = 1.0f;
    float fScaleZ = 1.0f;
    float fOffsetX = 0.5 + 1.0/32;
    float fOffsetY = 0.5 + 1.0/32;
    D3DVIEWPORT8 Viewport;

    
	EMUPATCH(D3DDevice_GetViewport)(&Viewport);
    


    pScale->x = 1.0f;
    pScale->y = 1.0f;
    pScale->z = 1.0f;
    pScale->w = 1.0f;

    pOffset->x = 0.0f;
    pOffset->y = 0.0f;
    pOffset->z = 0.0f;
    pOffset->w = 0.0f;

/*
    pScale->x = (float)Viewport.Width * 0.5f * fScaleX;
    pScale->y = (float)Viewport.Height * -0.5f * fScaleY;
    pScale->z = (Viewport.MaxZ - Viewport.MinZ) * fScaleZ;
    pScale->w = 0;

    pOffset->x = (float)Viewport.Width * fScaleX * 0.5f + (float)Viewport.X * fScaleX + fOffsetX;
    pOffset->y = (float)Viewport.Height * fScaleY * 0.5f + (float)Viewport.Y * fScaleY + fOffsetY;
    pOffset->z = Viewport.MinZ * fScaleZ;
    pOffset->w = 0;
*/

    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetShaderConstantMode)
(
    XTL::X_D3DSHADERCONSTANTMODE Mode
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Mode);

    g_VertexShaderConstantMode = Mode;
}

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_Reset)
(
    X_D3DPRESENT_PARAMETERS *pPresentationParameters
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pPresentationParameters);

	LOG_IGNORED();

    return D3D_OK;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetRenderTarget)
(
    X_D3DSurface  **ppRenderTarget
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_GetRenderTarget2");

	*ppRenderTarget = EMUPATCH(D3DDevice_GetRenderTarget2)();

    return D3D_OK; // Never returns D3DERR_NOTFOUND (D3DDevice_GetDepthStencilSurface does)
}
#endif

#if 0 // Patch disabled
XTL::X_D3DSurface * WINAPI XTL::EMUPATCH(D3DDevice_GetRenderTarget2)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	X_D3DSurface *result = g_pActiveXboxRenderTarget;
	if (result != NULL)
		result->Common++; // EMUPATCH(D3DResource_AddRef)(result) would give too much overhead (and needless logging)

    RETURN(result);
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetDepthStencilSurface)
(
    X_D3DSurface  **ppZStencilSurface
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_GetDepthStencilSurface2");

    *ppZStencilSurface = EMUPATCH(D3DDevice_GetDepthStencilSurface2)();

	if (*ppZStencilSurface == NULL)
		return D3DERR_NOTFOUND;

    return D3D_OK;
}
#endif

#if 0 // Patch disabled
XTL::X_D3DSurface * WINAPI XTL::EMUPATCH(D3DDevice_GetDepthStencilSurface2)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	X_D3DSurface *result = g_pActiveXboxDepthStencil;
	if (result != NULL)
		result->Common++; // EMUPATCH(D3DResource_AddRef)(result) would give too much overhead (and needless logging)
		
	RETURN(result);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetTile)
(
    DWORD           Index,
    X_D3DTILE      *pTile
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(pTile)
		LOG_FUNC_END;

	if (pTile != NULL) {
		memcpy(pTile, &EmuD3DTileCache[Index], sizeof(X_D3DTILE));
	}
}

// Dxbx note : SetTile is applied to SetTileNoWait in Cxbx 4361 OOPVA's!
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTile)
(
    DWORD               Index,
    CONST X_D3DTILE    *pTile
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(pTile)
		LOG_FUNC_END;

	if (pTile != NULL) {
		memcpy(&EmuD3DTileCache[Index], pTile, sizeof(X_D3DTILE));
	}
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateVertexShader)
(
    CONST DWORD    *pDeclaration,
    CONST DWORD    *pFunction,
    DWORD          *pHandle,
    DWORD           Usage
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pDeclaration)
		LOG_FUNC_ARG(pFunction)
		LOG_FUNC_ARG(pHandle)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_END;

    // HACK: TODO: support this situation
    if(pDeclaration == NULL) {
        *pHandle = NULL;

        return D3D_OK;
    }

    // create emulated shader struct
    X_D3DVertexShader *pXboxVertexShader = (X_D3DVertexShader*)g_MemoryManager.AllocateZeroed(1, sizeof(X_D3DVertexShader));
    CxbxVertexShader *pHostVertexShader = (CxbxVertexShader*)g_MemoryManager.AllocateZeroed(1, sizeof(CxbxVertexShader));

    // TODO: Intelligently fill out these fields as necessary

    LPD3DXBUFFER pRecompiledBuffer = nullptr;
    DWORD        *pRecompiledDeclaration = nullptr;
    DWORD        *pRecompiledFunction = nullptr;
    DWORD        VertexShaderSize = 0;
    DWORD        DeclarationSize = 0;
    DWORD        HostVertexHandle = 0;

    HRESULT hRet = XTL::EmuRecompileVshDeclaration((DWORD*)pDeclaration,
                                                   &pRecompiledDeclaration,
                                                   &DeclarationSize,
                                                   pFunction == NULL,
                                                   &pHostVertexShader->VertexShaderDynamicPatch);

	boolean bUseDeclarationOnly = false;

    if (SUCCEEDED(hRet) && pFunction) {
        hRet = XTL::EmuRecompileVshFunction((DWORD*)pFunction,
                                            &pRecompiledBuffer,
                                            &VertexShaderSize,
                                            g_VertexShaderConstantMode == X_D3DSCM_NORESERVEDCONSTANTS,
											&bUseDeclarationOnly);
        if (!SUCCEEDED(hRet)) {
            EmuWarning("Couldn't recompile vertex shader function.");
            hRet = D3D_OK; // Try using a fixed function vertex shader instead
        }
    }

    //DbgPrintf("MaxVertexShaderConst = %d\n", g_D3DCaps.MaxVertexShaderConst);

    if (SUCCEEDED(hRet)) {
		if (!bUseDeclarationOnly) {
			if (pRecompiledBuffer)
				pRecompiledFunction = (DWORD*)pRecompiledBuffer->GetBufferPointer();
		}

        hRet = g_pD3DDevice8->CreateVertexShader(
            pRecompiledDeclaration,
            pRecompiledFunction,
            &HostVertexHandle,
            g_dwVertexShaderUsage   // TODO: HACK: Xbox has extensions!
        );
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexShader");

        if (pRecompiledBuffer != nullptr) {
            pRecompiledBuffer->Release();
            pRecompiledBuffer = nullptr;
        }

        //* Fallback to dummy shader.
        if (FAILED(hRet)) {
            static const char dummy[] =
                "vs.1.1\n"
                "mov oPos, v0\n";
			XTL::LPD3DXBUFFER pCompilationErrors = nullptr;

            EmuWarning("Trying fallback:\n%s", dummy);
            hRet = D3DXAssembleShader(dummy,
                                      strlen(dummy),
                                      D3DXASM_SKIPVALIDATION,
                                      nullptr,
                                      &pRecompiledBuffer,
                                      &pCompilationErrors);
			DEBUG_D3DRESULT(hRet, "D3DXAssembleShader");

			if (pCompilationErrors) {
				EmuWarning((char*)pCompilationErrors->GetBufferPointer());
				pCompilationErrors->Release();
				pCompilationErrors = nullptr;
			}

			if (pRecompiledBuffer != nullptr) {
				hRet = g_pD3DDevice8->CreateVertexShader(
					pRecompiledDeclaration,
					(DWORD*)pRecompiledBuffer->GetBufferPointer(),
					&HostVertexHandle,
					g_dwVertexShaderUsage
				);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexShader(fallback)");

				pRecompiledBuffer->Release();
				pRecompiledBuffer = nullptr;
			}
		}
        //*/
    }

    free(pRecompiledDeclaration);

    // Save the status, to remove things later
    pHostVertexShader->Size = (VertexShaderSize - sizeof(VSH_SHADER_HEADER)) / VSH_INSTRUCTION_SIZE_BYTES;
    pHostVertexShader->pDeclaration = (DWORD*)g_MemoryManager.Allocate(DeclarationSize);
    memcpy(pHostVertexShader->pDeclaration, pDeclaration, DeclarationSize);
    pHostVertexShader->DeclarationSize = DeclarationSize;
    pHostVertexShader->pFunction = nullptr;
    pHostVertexShader->FunctionSize = 0;
    pHostVertexShader->Type = X_VST_NORMAL;
    pHostVertexShader->Status = hRet;

    if(SUCCEEDED(hRet)) {
        if(pFunction != NULL) {
            pHostVertexShader->pFunction = (DWORD*)g_MemoryManager.Allocate(VertexShaderSize);
            memcpy(pHostVertexShader->pFunction, pFunction, VertexShaderSize);
            pHostVertexShader->FunctionSize = VertexShaderSize;
        }

        pHostVertexShader->Handle = HostVertexHandle;
    }
    else {
        pHostVertexShader->Handle = D3DFVF_XYZ | D3DFVF_TEX0;
    }

	// (Ab)use the Xbox Handle for storing the host vertex shader
    pXboxVertexShader->Handle = (DWORD)pHostVertexShader;
	// Make sure the Xbox recognizes this handle as a vertex shader (not a FVF)
	*pHandle = (DWORD)pXboxVertexShader | D3DFVF_RESERVED0; // see VshHandleIsFVF and VshHandleIsVertexShader

    if(FAILED(hRet)) {
#ifdef _DEBUG_TRACK_VS
        if (pFunction != NULL) {
            char pFileName[30];
            static int FailedShaderCount = 0;
            VSH_SHADER_HEADER *pHeader = (VSH_SHADER_HEADER*)pFunction;
            EmuWarning("Couldn't create vertex shader!");
            sprintf(pFileName, "failed%05d.xvu", FailedShaderCount);
            FILE *f = fopen(pFileName, "wb");
            if (f != nullptr) {
                fwrite(pFunction, sizeof(VSH_SHADER_HEADER) + pHeader->NumInst * 16, 1, f);
                fclose(f);
            }

            FailedShaderCount++;
        }
#endif // _DEBUG_TRACK_VS
        //hRet = D3D_OK;
    }    

    RETURN(hRet);
}

#if 0 // patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetPixelShaderConstant)
(
    DWORD       Register,
    CONST PVOID pConstantData,
    DWORD       ConstantCount
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Register)
		LOG_FUNC_ARG(pConstantData)
		LOG_FUNC_ARG(ConstantCount)
		LOG_FUNC_END;

	// TODO: This hack is necessary for Vertex Shaders on XDKs prior to 4361, but if this
	// causes problems with pixel shaders, feel free to comment out the hack below.
	// HACK: Since Xbox vertex shader constants range from -96 to 95, during conversion
	// some shaders need to add 96 to use ranges 0 to 191.  This fixes 3911 - 4361 games and XDK
	// samples, but breaks Turok.
	// Dxbx note : 4627 samples show that the Register value arriving in this function is already
	// incremented with 96 (even though the code for these samples supplies 0, maybe there's a
	// macro responsible for that?)
	if (X_D3DSCM_CORRECTION_VersionDependent > 0) {
		Register += X_D3DSCM_CORRECTION_VersionDependent;
		DbgPrintf("Corrected constant register : 0x%.08x\n", Register);
	}

    HRESULT hRet = g_pD3DDevice8->SetPixelShaderConstant
    (
        Register,
        pConstantData,
        ConstantCount
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetPixelShaderConstant");

    if(FAILED(hRet))
    {
        EmuWarning("We're lying about setting a pixel shader constant!");

        hRet = D3D_OK;
    }
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexShaderConstant)
(
    INT         Register,
    CONST PVOID pConstantData,
    DWORD       ConstantCount
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Register)
		LOG_FUNC_ARG(pConstantData)
		LOG_FUNC_ARG(ConstantCount)
		LOG_FUNC_END;

/*#ifdef _DEBUG_TRACK_VS_CONST
    for (uint32 i = 0; i < ConstantCount; i++)
    {
        printf("SetVertexShaderConstant, c%d (c%d) = { %f, %f, %f, %f }\n",
               Register - 96 + i, Register + i,
               *((float*)pConstantData + 4 * i),
               *((float*)pConstantData + 4 * i + 1),
               *((float*)pConstantData + 4 * i + 2),
               *((float*)pConstantData + 4 * i + 3));
    }
#endif*/ // _DEBUG_TRACK_VS_CONST

	// TODO: HACK: Since Xbox vertex shader constants range from -96 to 96, during conversion
	// some shaders need to add 96 to use ranges 0 to 192.  This fixes 3911 - 4361 games and XDK
	// samples, but breaks Turok.

	if (X_D3DSCM_CORRECTION_VersionDependent > 0) {
		Register += X_D3DSCM_CORRECTION_VersionDependent;
		DbgPrintf("Corrected constant register : 0x%.08x\n", Register);
	}

    HRESULT hRet = g_pD3DDevice8->SetVertexShaderConstant
    (
        Register,
        pConstantData,
        ConstantCount
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShaderConstant");

    if(FAILED(hRet))
    {
        EmuWarning("We're lying about setting a vertex shader constant!");
        hRet = D3D_OK;
    }   
}

VOID __fastcall XTL::EMUPATCH(D3DDevice_SetVertexShaderConstant1)
(
    INT         Register,
    CONST PVOID pConstantData
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexShaderConstant");

    EMUPATCH(D3DDevice_SetVertexShaderConstant)(Register, pConstantData, 1);
}

VOID __fastcall XTL::EMUPATCH(D3DDevice_SetVertexShaderConstant4)
(
    INT         Register,
    CONST PVOID pConstantData
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexShaderConstant");

	EMUPATCH(D3DDevice_SetVertexShaderConstant)(Register, pConstantData, 4);
}

VOID __fastcall XTL::EMUPATCH(D3DDevice_SetVertexShaderConstantNotInline)
(
    INT         Register,
    CONST PVOID pConstantData,
    DWORD       ConstantCount
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexShaderConstant");

	EMUPATCH(D3DDevice_SetVertexShaderConstant)(Register, pConstantData, ConstantCount / 4);
}

#if 0 // patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_DeletePixelShader)
(
    DWORD          Handle
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Handle);

    if(Handle == X_PIXELSHADER_FAKE_HANDLE)
    {
        // Do Nothing!
    }
    else
    {
        HRESULT hRet = g_pD3DDevice8->DeletePixelShader(Handle);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DeletePixelShader");
	}

	/*CxbxPixelShader *pPixelShader = (CxbxPixelShader*)Handle;

	if (pPixelShader)
	{
		if(pPixelShader->Handle != X_PIXELSHADER_FAKE_HANDLE)
		{
			HRESULT hRet = g_pD3DDevice8->DeletePixelShader(pPixelShader->Handle);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DeletePixelShader");
		}

		g_MemoryManager.Free(pPixelShader);
	}*/
}
#endif

#if 0 // patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreatePixelShader)
(
    X_D3DPIXELSHADERDEF    *pPSDef,
    DWORD				   *pHandle
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pPSDef)
		LOG_FUNC_ARG(pHandle)
		LOG_FUNC_END;

	HRESULT hRet = E_FAIL;
#if 0 // PatrickvL Dxbx pixel shader translation

	// Attempt to recompile PixelShader
	hRet = DxbxUpdateActivePixelShader(pPSDef, pHandle);
	// redirect to windows d3d
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreatePixelShader");
#endif
#if 1 // Kingofc's pixel shader translation
	DWORD* pFunction = NULL;
	LPD3DXBUFFER pRecompiledBuffer = nullptr;
	// DWORD Handle = 0;

	hRet = CreatePixelShaderFunction(pPSDef, &pRecompiledBuffer);
	DEBUG_D3DRESULT(hRet, "CreatePixelShaderFunction");

	if (SUCCEEDED(hRet))
	{
		pFunction = (DWORD*)pRecompiledBuffer->GetBufferPointer();

		// Redirect to Windows D3D
		hRet = g_pD3DDevice8->CreatePixelShader
		(
			pFunction,
			pHandle
			/*&Handle*/
		);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreatePixelShader");
	}

	if (pRecompiledBuffer != nullptr)
	{
		pRecompiledBuffer->Release();
	}

	// This additional layer of Cxbx internal indirection seems problematic, as 
	// CreatePixelShader() is expected to return a pHandle directly to a shader interface.

	/*
	CxbxPixelShader *pPixelShader = (CxbxPixelShader*)g_MemoryManager.AllocateZeroed(1, sizeof(CxbxPixelShader)); // Clear, to prevent side-effects on random contents

	memcpy(&pPixelShader->PSDef, pPSDef, sizeof(X_D3DPIXELSHADERDEF));

	DWORD Handle = 0; // ??
	pPixelShader->Handle = Handle;
	pPixelShader->dwStatus = hRet;
	*pHandle = (DWORD)pPixelShader;
	*/
#endif
#if 0 // Older Cxbx pixel shader translation
	DWORD* pFunction = NULL;

	pFunction = (DWORD*) pPSDef;

	// Attempt to recompile PixelShader
	EmuRecompilePshDef( pPSDef, NULL );

    // redirect to windows d3d
    hRet = g_pD3DDevice8->CreatePixelShader
    (
        pFunction,
        pHandle
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreatePixelShader");
#endif

    if(FAILED(hRet))
    {
        *pHandle = X_PIXELSHADER_FAKE_HANDLE;

		// This is called too frequently as Azurik creates and destroys a
		// pixel shader every frame, and makes debugging harder.
		// EmuWarning("We're lying about the creation of a pixel shader!");

        hRet = D3D_OK;
    }
    else
    {
        DbgPrintf("pHandle = 0x%.08X (0x%.08X)\n", pHandle, *pHandle);
    }

    

    return hRet;
}
#endif

#if 0 // patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetPixelShader)
(
    DWORD           Handle
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Handle);

    // Redirect to Windows D3D
    HRESULT hRet = D3D_OK;

    // Fake Programmable Pipeline
    if(Handle == X_PIXELSHADER_FAKE_HANDLE)
    {
        // programmable pipeline
        //*
        static DWORD dwHandle = 0;

        if(dwHandle == 0)
        {
            // simplest possible pixel shader, simply output the texture input
            static const char szDiffusePixelShader[] =
                "ps.1.0\n"
                "tex t0\n"
                "mov r0, t0\n";

            LPD3DXBUFFER pShader = 0;
            LPD3DXBUFFER pErrors = 0;

            // assemble the shader
            D3DXAssembleShader(szDiffusePixelShader, strlen(szDiffusePixelShader) - 1, 0, nullptr, &pShader, &pErrors);

            // create the shader device handle
            hRet = g_pD3DDevice8->CreatePixelShader((DWORD*)pShader->GetBufferPointer(), &dwHandle);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreatePixelShader");
			g_dwCurrentPixelShader = 0;
        }

		if (SUCCEEDED(hRet))
		{
			hRet = g_pD3DDevice8->SetPixelShader(g_FillModeOverride == 0 ? dwHandle : NULL);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetPixelShader");
		}

        //*/

        g_bFakePixelShaderLoaded = TRUE;
    }
    // Fixed Pipeline, or Recompiled Programmable Pipeline
    else if(Handle != NULL)
    {
        EmuWarning("Trying fixed or recompiled programmable pipeline pixel shader!");
        g_bFakePixelShaderLoaded = FALSE;
		g_dwCurrentPixelShader = Handle;
        hRet = g_pD3DDevice8->SetPixelShader(g_FillModeOverride == 0 ? Handle : NULL);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetPixelShader(fixed)");
	}

    if(FAILED(hRet))
    {
        EmuWarning("We're lying about setting a pixel shader!");

        hRet = D3D_OK;
    }
}
#endif

#if 0 // Patch disabled
XTL::X_D3DResource * WINAPI XTL::EMUPATCH(D3DDevice_CreateTexture2)
(
    UINT                Width,
    UINT                Height,
    UINT                Depth,
    UINT                Levels,
    DWORD               Usage,
    X_D3DFORMAT         Format,
    X_D3DRESOURCETYPE   D3DResource
)
{
//	FUNC_EXPORTS

    X_D3DTexture *pTexture = NULL;

    switch(D3DResource)
    {
	case X_D3DRTYPE_TEXTURE: {
		LOG_FORWARD("D3DDevice_CreateTexture");
		EMUPATCH(D3DDevice_CreateTexture)(Width, Height, Levels, Usage, Format, D3DPOOL_MANAGED, &pTexture);
		break;
	}
	case X_D3DRTYPE_VOLUMETEXTURE: {
		LOG_FORWARD("D3DDevice_CreateVolumeTexture");
		EMUPATCH(D3DDevice_CreateVolumeTexture)(Width, Height, Depth, Levels, Usage, Format, D3DPOOL_MANAGED, (X_D3DVolumeTexture**)&pTexture);
		break;
	}
	case X_D3DRTYPE_CUBETEXTURE: {
		LOG_FORWARD("D3DDevice_CreateCubeTexture");
		//DbgPrintf( "D3DDevice_CreateTexture2: Width = 0x%X, Height = 0x%X\n", Width, Height );
		//CxbxKrnlCleanup("Cube textures temporarily not supported!");
		EMUPATCH(D3DDevice_CreateCubeTexture)(Width, Levels, Usage, Format, D3DPOOL_MANAGED, (X_D3DCubeTexture**)&pTexture);
		break;
	}
	default:
		CxbxKrnlCleanup("D3DResource = %d is not supported!", D3DResource);
    }

    return pTexture;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateTexture)
(
    UINT            Width,
    UINT            Height,
    UINT            Levels,
    DWORD           Usage,
    X_D3DFORMAT     Format,
    D3DPOOL         Pool,
    X_D3DTexture  **ppTexture
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Width)
		LOG_FUNC_ARG(Height)
		LOG_FUNC_ARG(Levels)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_ARG(Pool)
		LOG_FUNC_ARG(ppTexture)
		LOG_FUNC_END;

	// Get Bytes Per Pixel, for correct Pitch calculation :
	DWORD dwBPP = EmuXBFormatBytesPerPixel(Format);

	UINT Pitch = RoundUp(Width, 64) * dwBPP; // TODO : RoundUp only for X_D3DFMT_YUY2?

    HRESULT hRet;

	X_D3DTexture *pTexture = EmuNewD3DTexture();
	IDirect3DTexture8 *pNewHostTexture = nullptr;
	DWORD Texture_Data;

    if(Format == X_D3DFMT_YUY2)
    {
        // cache the overlay size
        g_dwOverlayW = Width;
        g_dwOverlayH = Height;
        g_dwOverlayP = Pitch;

        Texture_Data = (DWORD)g_MemoryManager.Allocate(g_dwOverlayP * g_dwOverlayH);

        g_pCachedYuvSurface = (X_D3DSurface*)pTexture;

        hRet = D3D_OK;
    }
    else
    {
		// Convert Format (Xbox->PC)
		D3DFORMAT PCFormat = EmuXB2PC_D3DFormat(Format);

		// TODO: HACK: Devices that don't support this should somehow emulate it!
		//* This is OK on my GeForce FX 5600
		if(PCFormat == D3DFMT_D16)
		{
			EmuWarning("D3DFMT_D16 is an unsupported texture format!");
			PCFormat = D3DFMT_R5G6B5;
		}
		//*
		else if(PCFormat == D3DFMT_P8 && !IsResourceFormatSupported(X_D3DRTYPE_TEXTURE, D3DFMT_P8, 0))
		{
			EmuWarning("D3DFMT_P8 is an unsupported texture format!");
			PCFormat = D3DFMT_L8;
		}
		//*/
		//* This is OK on my GeForce FX 5600
		else if(PCFormat == D3DFMT_D24S8)
		{
			EmuWarning("D3DFMT_D24S8 is an unsupported texture format!");
			PCFormat = D3DFMT_X8R8G8B8; // TODO : Use D3DFMT_A8R8G8B8?
		}//*/
		// TODO: HACK: This texture format fails on some newer hardware
		else if(PCFormat == D3DFMT_X1R5G5B5)
		{
			EmuWarning("D3DFMT_X1R5G5B5 -> D3DFMT_R5G6B5");
			PCFormat = D3DFMT_R5G6B5;
		}

        DWORD   PCUsage = Usage & (D3DUSAGE_RENDERTARGET);
//        DWORD   PCUsage = Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL);
        D3DPOOL PCPool  = D3DPOOL_MANAGED;

		// DIRTY HACK: Render targets. The D3DUSAGE_RENDERTARGET
		// flag isn't always set by the XDK (if ever).
		/*if( Width != 640 && Height != 480 )
		{
		//	EmuAdjustPower2(&Width, &Height);
		}
		else
		{
			PCUsage = D3DUSAGE_RENDERTARGET;
			PCPool = D3DPOOL_DEFAULT;
		}*/

//		EmuAdjustPower2(&Width, &Height);

//        if(Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL))
        if(Usage & (D3DUSAGE_RENDERTARGET))
        {
			PCPool = D3DPOOL_DEFAULT;
        }

        hRet = g_pD3DDevice8->CreateTexture
        (
            Width, Height, Levels,
            PCUsage,  // TODO: Xbox Allows a border to be drawn (maybe hack this in software ;[)
            PCFormat, PCPool, &pNewHostTexture
        );
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateTexture");

        if(FAILED(hRet))
        {
            //EmuWarning("CreateTexture Failed!");
			EmuWarning("CreateTexture Failed!\n\n"
								"Error: 0x%X\nFormat: %d\nDimensions: %dx%d", hRet, PCFormat, Width, Height);
			Texture_Data = 0xBEADBEAD;
        }
        else
        {
			SetHostTexture(pTexture, pNewHostTexture);

            /**
             * Note: If CreateTexture() called with D3DPOOL_DEFAULT then unable to Lock. 
             * It will cause an Error with the DirectX Debug runtime.
             *
             * This is most commonly seen with
             *      D3DUSAGE_RENDERTARGET or
             *      D3DUSAGE_DEPTHSTENCIL
             * that can only be used with D3DPOOL_DEFAULT per MSDN.
             */
            D3DLOCKED_RECT LockedRect;

			pNewHostTexture->LockRect(0, &LockedRect, nullptr, D3DLOCK_READONLY);
			Texture_Data = (DWORD)LockedRect.pBits;
            g_DataToTexture.insert(Texture_Data, pTexture);
			pNewHostTexture->UnlockRect(0);
        }
    }

	// Set all X_D3DTexture members (except Lock)
	EMUPATCH(XGSetTextureHeader)(Width, Height, Levels, Usage, Format, Pool, pTexture, Texture_Data, Pitch);

	DbgPrintf("D3D8: Created Texture : 0x%.08X (0x%.08X)\n", pTexture, pNewHostTexture);

	*ppTexture = pTexture;

    return hRet;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateVolumeTexture)
(
    UINT                 Width,
    UINT                 Height,
    UINT                 Depth,
    UINT                 Levels,
    DWORD                Usage,
    X_D3DFORMAT          Format,
    D3DPOOL              Pool,
    X_D3DVolumeTexture **ppVolumeTexture
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Width)
		LOG_FUNC_ARG(Height)
		LOG_FUNC_ARG(Depth)
		LOG_FUNC_ARG(Levels)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_ARG(Pool)
		LOG_FUNC_ARG(ppVolumeTexture)
		LOG_FUNC_END;

    *ppVolumeTexture = EmuNewD3DVolumeTexture();

    HRESULT hRet;

    if(Format == X_D3DFMT_YUY2)
    {
        // cache the overlay size
        g_dwOverlayW = Width;
        g_dwOverlayH = Height;
        g_dwOverlayP = RoundUp(g_dwOverlayW, 64)*2;

        (*ppVolumeTexture)->Data = (DWORD)g_MemoryManager.Allocate(g_dwOverlayP * g_dwOverlayH);
		(*ppVolumeTexture)->Format = Format << X_D3DFORMAT_FORMAT_SHIFT;

        (*ppVolumeTexture)->Size = (g_dwOverlayW & X_D3DSIZE_WIDTH_MASK)
                                 | (g_dwOverlayH << X_D3DSIZE_HEIGHT_SHIFT)
                                 | (g_dwOverlayP << X_D3DSIZE_PITCH_SHIFT);

        hRet = D3D_OK;
    }
    else
    {
		// Convert Format (Xbox->PC)
		D3DFORMAT PCFormat = EmuXB2PC_D3DFormat(Format);

		// TODO: HACK: Devices that don't support this should somehow emulate it!
		if (PCFormat == D3DFMT_D16)
		{
			EmuWarning("D3DFMT_D16 is an unsupported texture format!");
			PCFormat = D3DFMT_X8R8G8B8; // TODO : Use D3DFMT_R5G6B5 ?
		}
		else if (PCFormat == D3DFMT_P8 && !IsResourceFormatSupported(X_D3DRTYPE_VOLUMETEXTURE, D3DFMT_P8, 0))
		{
			EmuWarning("D3DFMT_P8 is an unsupported texture format!");
			PCFormat = D3DFMT_L8;
		}
		else if (PCFormat == D3DFMT_D24S8)
		{
			EmuWarning("D3DFMT_D24S8 is an unsupported texture format!");
			PCFormat = D3DFMT_X8R8G8B8; // TODO : Use D3DFMT_A8R8G8B8?
		}

		EmuAdjustPower2(&Width, &Height);

		XTL::IDirect3DVolumeTexture8 *pNewHostVolumeTexture = nullptr;

        hRet = g_pD3DDevice8->CreateVolumeTexture
        (
            Width, Height, Depth, Levels,
            0,  // TODO: Xbox Allows a border to be drawn (maybe hack this in software ;[)
            PCFormat, D3DPOOL_MANAGED, &pNewHostVolumeTexture
        );
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVolumeTexture");

        if(SUCCEEDED(hRet))
		{
			SetHostVolumeTexture(*ppVolumeTexture, pNewHostVolumeTexture);
			DbgPrintf("D3D8: Created Volume Texture : 0x%.08X (0x%.08X)\n", *ppVolumeTexture, pNewHostVolumeTexture);
		}
    }

    return hRet;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateCubeTexture)
(
    UINT                 EdgeLength,
    UINT                 Levels,
    DWORD                Usage,
    X_D3DFORMAT          Format,
    D3DPOOL              Pool,
    X_D3DCubeTexture  **ppCubeTexture
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(EdgeLength)
		LOG_FUNC_ARG(Levels)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_ARG(Pool)
		LOG_FUNC_ARG(ppCubeTexture)
		LOG_FUNC_END;

	if(Format == X_D3DFMT_YUY2)
    {
        CxbxKrnlCleanup("YUV not supported for cube textures");
    }

    // Convert Format (Xbox->PC)
    D3DFORMAT PCFormat = EmuXB2PC_D3DFormat(Format);

    // TODO: HACK: Devices that don't support this should somehow emulate it!
    if(PCFormat == D3DFMT_D16)
    {
        EmuWarning("D3DFMT_D16 is an unsupported texture format!");
        PCFormat = D3DFMT_X8R8G8B8; // TODO : Use D3DFMT_R5G6B5?
    }
    else if(PCFormat == D3DFMT_P8 && !IsResourceFormatSupported(X_D3DRTYPE_CUBETEXTURE, D3DFMT_P8, 0))
    {
        EmuWarning("D3DFMT_P8 is an unsupported texture format!");
        PCFormat = D3DFMT_L8;
    }
    else if(PCFormat == D3DFMT_D24S8)
    {
        EmuWarning("D3DFMT_D24S8 is an unsupported texture format!");
        PCFormat = D3DFMT_X8R8G8B8; // TODO : Use D3DFMT_A8R8G8B8?
    }
    
    *ppCubeTexture = EmuNewD3DCubeTexture();
	XTL::IDirect3DCubeTexture8 *pNewHostCubeTexture = nullptr;

    HRESULT hRet = g_pD3DDevice8->CreateCubeTexture
    (
        EdgeLength, Levels,
        0,  // TODO: Xbox Allows a border to be drawn (maybe hack this in software ;[)
        PCFormat, D3DPOOL_MANAGED, &pNewHostCubeTexture
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateCubeTexture");

    if(SUCCEEDED(hRet))
	{
		SetHostCubeTexture(*ppCubeTexture, pNewHostCubeTexture);
		DbgPrintf("D3D8: Created Cube Texture : 0x%.08X (0x%.08X)\n", *ppCubeTexture, pNewHostCubeTexture);
	}    

    return hRet;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateIndexBuffer)
(
    UINT                 Length,
    DWORD                Usage,
    X_D3DFORMAT          Format,
    D3DPOOL              Pool,
    X_D3DIndexBuffer   **ppIndexBuffer
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG_TYPE(X_D3DUSAGE, Usage)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_ARG(Pool)
		LOG_FUNC_ARG(ppIndexBuffer)
		LOG_FUNC_END;

	if (Format != X_D3DFMT_INDEX16)
		EmuWarning("CreateIndexBuffer called with unexpected format! (0x%.08X)", Format);

    *ppIndexBuffer = EmuNewD3DIndexBuffer();

	XTL::IDirect3DIndexBuffer8 *pNewHostIndexBuffer = nullptr;

	HRESULT hRet = g_pD3DDevice8->CreateIndexBuffer
    (
        Length/*InBytes*/, /*Usage=*/0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pNewHostIndexBuffer
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateIndexBuffer");
    if(SUCCEEDED(hRet))
	{
		SetHostIndexBuffer(*ppIndexBuffer, pNewHostIndexBuffer);
		DbgPrintf("D3DDevice_CreateIndexBuffer: pHostIndexBuffer := 0x%.08X\n", pNewHostIndexBuffer);
	}

    // update data ptr
    {
        BYTE *pNativeData = nullptr;

		hRet = pNewHostIndexBuffer->Lock(/*OffsetToLock=*/0, Length, &pNativeData, /*Flags=*/0);
		DEBUG_D3DRESULT(hRet, "pNewHostIndexBuffer->Lock");

		if(FAILED(hRet))
			CxbxKrnlCleanup("IndexBuffer Lock Failed!);

        (*ppIndexBuffer)->Data = (DWORD)pNativeData; // For now, give the native buffer memory to Xbox. TODO : g_MemoryManager.AllocateContiguous
    }

    return hRet;
}
#endif

#if 0 // Patch disabled
XTL::X_D3DIndexBuffer * WINAPI XTL::EMUPATCH(D3DDevice_CreateIndexBuffer2)
(
	UINT Length
)
{
//	FUNC_EXPORTS

    X_D3DIndexBuffer *pIndexBuffer = NULL;

	LOG_FORWARD("D3DDevice_CreateIndexBuffer");

	EMUPATCH(D3DDevice_CreateIndexBuffer)
    (
        Length/*InBytes*/,
        /*Usage=*/0,
        X_D3DFMT_INDEX16,
        D3DPOOL_MANAGED,
        &pIndexBuffer
    );

    return pIndexBuffer;
}

#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_SetIndices)
(
    X_D3DIndexBuffer   *pIndexData,
    UINT                BaseVertexIndex
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pIndexData)
		LOG_FUNC_ARG(BaseVertexIndex)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;

    g_dwBaseVertexIndex = BaseVertexIndex;
	g_pIndexBuffer = pIndexData;

    return hRet;
}

#endif

#if 0 // Patch disabled - Pushes NV2A_TX_OFFSET, NV2A_TX_FORMAT and NV2A_TX_ENABLE, which we'll handle later but ignore for now
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTexture)
(
    DWORD           Stage,
    X_D3DBaseTexture  *pTexture
)
{
	FUNC_EXPORTS

	if (pTexture) {
		if (!IsAddressAllocated(pTexture)) {
			CxbxPopupMessage("D3DDevice_SetTexture : Unexpected texture addresss!");
			pTexture = NULL; // Prevent logging crash
		}
	}

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Stage)
		LOG_FUNC_ARG(pTexture)
		LOG_FUNC_END;

	if (pTexture)
		pTexture->Common++; // EMUPATCH(D3DResource_AddRef)(pTexture) would give too much overhead (and needless logging)

	X_D3DBaseTexture  *pOldTexture = GetXboxBaseTexture(Stage);
	SetXboxBaseTexture(Stage, pTexture); // Note : Is reference-counting needed?

	if (pOldTexture) {
		pOldTexture->Common--; // NOTE : Reaching zero should lead to destruction - we igore that for testing purposes
	}

#if 0
	IDirect3DBaseTexture8 *pHostBaseTexture = nullptr;

	if(pTexture != NULL)
    {
		pHostBaseTexture = CxbxUpdateTexture(pTexture, g_pTexturePaletteStages[Stage]);

#ifdef _DEBUG_DUMP_TEXTURE_SETTEXTURE
        if(pTexture != NULL && (pHostBaseTexture != nullptr))
        {
            static int dwDumpTexture = 0;

            char szBuffer[256];

            switch(pHostBaseTexture->GetType())
            {
                case D3DRTYPE_TEXTURE:
                {
                    sprintf(szBuffer, _DEBUG_DUMP_TEXTURE_SETTEXTURE "SetTextureNorm - %.03d (0x%.08X).bmp", dwDumpTexture++, pHostBaseTexture);

					((IDirect3DTexture8 *)pHostBaseTexture)->UnlockRect(0);

                    D3DXSaveTextureToFile(szBuffer, D3DXIFF_BMP, pHostBaseTexture, NULL);
                }
                break;

                case D3DRTYPE_CUBETEXTURE:
                {
                    for(int face=0;face<6;face++)
                    {
                        sprintf(szBuffer, _DEBUG_DUMP_TEXTURE_SETTEXTURE "SetTextureCube%d - %.03d (0x%.08X).bmp", face, dwDumpTexture++, pHostBaseTexture);

						((IDirect3DCubeTexture8 *)pHostBaseTexture)->UnlockRect((D3DCUBEMAP_FACES)face, 0);

                        D3DXSaveTextureToFile(szBuffer, D3DXIFF_BMP, pHostBaseTexture, NULL);
                    }
                }
                break;
            }
        }
#endif
    }

    /*
    static IDirect3DTexture8 *pDummyTexture[X_D3DTSS_STAGECOUNT] = {nullptr, nullptr, nullptr, nullptr};

    if(pDummyTexture[Stage] == nullptr)
    {
        if(Stage == 0)
        {
            if(D3DXCreateTextureFromFile(g_pD3DDevice8, "C:\\dummy1.bmp", &pDummyTexture[Stage]) != D3D_OK)
                CxbxKrnlCleanup("Could not create dummy texture!");
        }
        else if(Stage == 1)
        {
            if(D3DXCreateTextureFromFile(g_pD3DDevice8, "C:\\dummy2.bmp", &pDummyTexture[Stage]) != D3D_OK)
                CxbxKrnlCleanup("Could not create dummy texture!");
        }
    }
    //*/

    /*
    static int dwDumpTexture = 0;
    char szBuffer[256];
    sprintf(szBuffer, "C:\\Aaron\\Textures\\DummyTexture - %.03d (0x%.08X).bmp", dwDumpTexture++, pDummyTexture);
    pDummyTexture->UnlockRect(0);
    D3DXSaveTextureToFile(szBuffer, D3DXIFF_BMP, pDummyTexture, NULL);
    //*/

    //HRESULT hRet = g_pD3DDevice8->SetTexture(Stage, pDummyTexture[Stage]);
    HRESULT hRet = g_pD3DDevice8->SetTexture(Stage, (g_FillModeOverride == 0) ? pHostBaseTexture : nullptr);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetTexture");
#endif
}
#endif

#if 0 // Patch disabled - We read the result from Xbox_D3DDevice_m_pTextures anyway. The pushed (PGRAPH) commands are currently ignored in our puller.
VOID __fastcall XTL::EMUPATCH(D3DDevice_SwitchTexture)
(
    DWORD           Method,
    PVOID           Data,
    DWORD           Format
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Method)
		LOG_FUNC_ARG(Data)
		LOG_FUNC_ARG(Format)
		LOG_FUNC_END;

    static const DWORD StageLookup[X_D3DTSS_STAGECOUNT] = { NV2A_TX_OFFSET(0), NV2A_TX_OFFSET(1), NV2A_TX_OFFSET(2), NV2A_TX_OFFSET(3) };
    DWORD Stage = -1;

	for (int s = 0; s < X_D3DTSS_STAGECOUNT; s++)
		if (StageLookup[s] == PUSH_METHOD(Method))
			Stage = s;

    if(Stage == -1)
    {
        CxbxKrnlCleanup("D3DDevice_SwitchTexture : Unknown Method (0x%.08X)", Method);
    }
    else
    {
		X_D3DTexture *pTexture = NULL;
		IDirect3DBaseTexture8 *pHostBaseTexture = nullptr;

		if (Data != NULL)
		{
			pTexture = (X_D3DTexture *)g_DataToTexture.get(Data);
			if (pTexture != NULL) {
				pTexture->Common++; // AddRef
				pHostBaseTexture = GetHostBaseTexture(pTexture);
			}
		}

		X_D3DBaseTexture *pOldTexture = GetXboxBaseTexture(Stage);

		SetXboxBaseTexture(Stage, pTexture);

		if (pOldTexture != NULL) {
			if (pOldTexture->Common-- <= 1) {
				// TODO : Call unpatched Release to prevent memory leak
				LOG_TEST_CASE("Leaked a replaced texture");
			}
		}

		DbgPrintf("Switching Data 0x%.08X Texture 0x%.08X (0x%.08X) @ Stage %d\n", Data, pTexture, pHostBaseTexture, Stage);

        HRESULT hRet = g_pD3DDevice8->SetTexture(Stage, pHostBaseTexture);

        /*
        if(GetHostBaseTexture(pTexture) != nullptr)
        {
            static int dwDumpTexture = 0;

            char szBuffer[255];

            sprintf(szBuffer, "C:\\Aaron\\Textures\\0x%.08X-SwitchTexture%.03d.bmp", pTexture, dwDumpTexture++);

            pHostBaseTexture->UnlockRect(0);

            D3DXSaveTextureToFile(szBuffer, D3DXIFF_BMP, pHostBaseTexture, NULL);
        }
        //*/
    }
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetDisplayMode)
(
    OUT X_D3DDISPLAYMODE         *pMode
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG_OUT(pMode);

    HRESULT hRet;

    // make adjustments to parameters to make sense with windows d3d
    {
        D3DDISPLAYMODE *pPCMode = (D3DDISPLAYMODE*)pMode;

        hRet = g_pD3DDevice8->GetDisplayMode(pPCMode);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetDisplayMode");

		// Convert Format (PC->Xbox)
		pMode->Format = EmuPC2XB_D3DFormat(pPCMode->Format); // TODO : g_EmuCDPD.XboxPresentationParameters.BackBufferFormat;

        // TODO: Make this configurable in the future?
        pMode->Flags  = 0x000000A1; // D3DPRESENTFLAG_FIELD | D3DPRESENTFLAG_INTERLACED | D3DPRESENTFLAG_LOCKABLE_BACKBUFFER

		// TODO: Retrieve from current CreateDevice settings?
		pMode->Width = 640; // TODO : g_EmuCDPD.XboxPresentationParameters.BackBufferWidth;
		pMode->Height = 480; // TODO : g_EmuCDPD.XboxPresentationParameters.BackBufferHeight;
		pMode->RefreshRate = 50; // New
    }
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_Begin)
(
    X_D3DPRIMITIVETYPE     PrimitiveType
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(PrimitiveType);

    g_InlineVertexBuffer_PrimitiveType = PrimitiveType;
    g_InlineVertexBuffer_TableOffset = 0;
    g_InlineVertexBuffer_FVF = 0;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexData2f)
(
    int     Register,
    FLOAT   a,
    FLOAT   b
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexData4f");

    EMUPATCH(D3DDevice_SetVertexData4f)(Register, a, b, 0.0f, 1.0f);
}

static inline DWORD FtoDW(FLOAT f) { return *((DWORD*)&f); }
static inline FLOAT DWtoF(DWORD f) { return *((FLOAT*)&f); }

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexData2s)
(
    int     Register,
    SHORT   a,
    SHORT   b
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexData4f");

    DWORD dwA = a, dwB = b;

    EMUPATCH(D3DDevice_SetVertexData4f)(Register, DWtoF(dwA), DWtoF(dwB), 0.0f, 1.0f);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexData4f)
(
    int     Register,
    FLOAT   a,
    FLOAT   b,
    FLOAT   c,
    FLOAT   d
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Register)
		LOG_FUNC_ARG(a)
		LOG_FUNC_ARG(b)
		LOG_FUNC_ARG(c)
		LOG_FUNC_ARG(d)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;


	// Grow g_InlineVertexBuffer_Table to contain at least current, and a potentially next vertex
	if (g_InlineVertexBuffer_TableLength <= g_InlineVertexBuffer_TableOffset + 1) {
		if (g_InlineVertexBuffer_TableLength == 0) {
			g_InlineVertexBuffer_TableLength = PAGE_SIZE / sizeof(struct _D3DIVB);
		} else {
			g_InlineVertexBuffer_TableLength *= 2;
		}

		g_InlineVertexBuffer_Table = (struct _D3DIVB*)realloc(g_InlineVertexBuffer_Table, sizeof(struct _D3DIVB) * g_InlineVertexBuffer_TableLength);
		DbgPrintf("Reallocated g_InlineVertexBuffer_Table to %d entries\n", g_InlineVertexBuffer_TableLength);
	}

	// Is this the initial call after D3DDevice_Begin() ?
	if (g_InlineVertexBuffer_FVF == 0) {
		// Set first vertex to zero (preventing leaks from prior Begin/End calls)
		g_InlineVertexBuffer_Table[0] = {};
	}

	int o = g_InlineVertexBuffer_TableOffset;

	switch(Register)
    {
		case X_D3DVSDE_VERTEX:
		case X_D3DVSDE_POSITION:
        {
            g_InlineVertexBuffer_Table[o].Position.x = a;
            g_InlineVertexBuffer_Table[o].Position.y = b;
            g_InlineVertexBuffer_Table[o].Position.z = c;
            g_InlineVertexBuffer_Table[o].Rhw = d; // Was : 1.0f; // Dxbx note : Why set Rhw to 1.0? And why ignore d?

			g_InlineVertexBuffer_FVF |= D3DFVF_XYZRHW;
	        break;
        }

		case X_D3DVSDE_BLENDWEIGHT:
		{
			g_InlineVertexBuffer_Table[o].Blend1 = a;
			g_InlineVertexBuffer_Table[o].Blend2 = b;
			g_InlineVertexBuffer_Table[o].Blend3 = c;
			g_InlineVertexBuffer_Table[o].Blend4 = d;

			g_InlineVertexBuffer_FVF |= D3DFVF_XYZB1; // TODO : Detect D3DFVF_XYZB2, D3DFVF_XYZB3 or D3DFVF_XYZB4
		    break;
        }

		case X_D3DVSDE_NORMAL:
        {
            g_InlineVertexBuffer_Table[o].Normal.x = a;
            g_InlineVertexBuffer_Table[o].Normal.y = b;
            g_InlineVertexBuffer_Table[o].Normal.z = c;

            g_InlineVertexBuffer_FVF |= D3DFVF_NORMAL;
			break;
        }

        case X_D3DVSDE_DIFFUSE:
        {
            DWORD ca = FtoDW(d) << 24;
            DWORD cr = FtoDW(a) << 16;
            DWORD cg = FtoDW(b) << 8;
            DWORD cb = FtoDW(c) << 0;

            g_InlineVertexBuffer_Table[o].Diffuse = ca | cr | cg | cb;

            g_InlineVertexBuffer_FVF |= D3DFVF_DIFFUSE;
			break;
        }

		case X_D3DVSDE_SPECULAR:
        {
            DWORD ca = FtoDW(d) << 24;
            DWORD cr = FtoDW(a) << 16;
            DWORD cg = FtoDW(b) << 8;
            DWORD cb = FtoDW(c) << 0;

            g_InlineVertexBuffer_Table[o].Specular = ca | cr | cg | cb;

            g_InlineVertexBuffer_FVF |= D3DFVF_SPECULAR;
			break;
        }
#if 0
		case X_D3DVSDE_FOG:
		{
			LOG_TEST_CASE("X_D3DVSDE_FOG");

			DWORD ca = FtoDW(d) << 24;
			DWORD cr = FtoDW(a) << 16;
			DWORD cg = FtoDW(b) << 8;
			DWORD cb = FtoDW(c) << 0;

			g_InlineVertexBuffer_Table[o].Fog = ca | cr | cg | cb;
		}
#endif
		case X_D3DVSDE_BACKDIFFUSE:
		{
			LOG_TEST_CASE("X_D3DVSDE_BACKDIFFUSE");
#if 0
			DWORD ca = FtoDW(d) << 24;
			DWORD cr = FtoDW(a) << 16;
			DWORD cg = FtoDW(b) << 8;
			DWORD cb = FtoDW(c) << 0;

			g_InlineVertexBuffer_Table[o].BackDiffuse = ca | cr | cg | cb;

			g_InlineVertexBuffer_FVF |= D3DFVF_DIFFUSE; // TODO : There's no D3DFVF_BACKDIFFUSE - how to handel?
#endif
			// TODO : Support backside rendering ...
			break;
		}
		case X_D3DVSDE_BACKSPECULAR:
		{
			LOG_TEST_CASE("X_D3DVSDE_BACKSPECULAR");
#if 0
			DWORD ca = FtoDW(d) << 24;
			DWORD cr = FtoDW(a) << 16;
			DWORD cg = FtoDW(b) << 8;
			DWORD cb = FtoDW(c) << 0;

			g_InlineVertexBuffer_Table[o].BackSpecular = ca | cr | cg | cb;

			g_InlineVertexBuffer_FVF |= D3DFVF_SPECULAR; // TODO : There's no D3DFVF_BACKSPECULAR - how to handel?
#endif
			// TODO : Support backside rendering ...
			break;
		}

		case X_D3DVSDE_TEXCOORD0:
        {
            g_InlineVertexBuffer_Table[o].TexCoord1.x = a;
            g_InlineVertexBuffer_Table[o].TexCoord1.y = b;
			g_InlineVertexBuffer_Table[o].TexCoord1.z = c;

            if( (g_InlineVertexBuffer_FVF & D3DFVF_TEXCOUNT_MASK) < D3DFVF_TEX1)
            {
				// Dxbx fix : Use mask, else the format might get expanded incorrectly :
				g_InlineVertexBuffer_FVF = (g_InlineVertexBuffer_FVF & ~D3DFVF_TEXCOUNT_MASK) | D3DFVF_TEX1;
				// Dxbx note : Correct usage of D3DFVF_TEX1 (and the other cases below)
				// can be tested with "Daphne Xbox" (the Laserdisc Arcade Game Emulator).
			}

			break;
        }

		case X_D3DVSDE_TEXCOORD1:
        {
            g_InlineVertexBuffer_Table[o].TexCoord2.x = a;
            g_InlineVertexBuffer_Table[o].TexCoord2.y = b;
			g_InlineVertexBuffer_Table[o].TexCoord2.z = c;

            if( (g_InlineVertexBuffer_FVF & D3DFVF_TEXCOUNT_MASK) < D3DFVF_TEX2)
            {
				// Dxbx fix : Use mask, else the format might get expanded incorrectly :
				g_InlineVertexBuffer_FVF = (g_InlineVertexBuffer_FVF & ~D3DFVF_TEXCOUNT_MASK) | D3DFVF_TEX2;
			}

			break;
        }

		case X_D3DVSDE_TEXCOORD2:
        {
            g_InlineVertexBuffer_Table[o].TexCoord3.x = a;
            g_InlineVertexBuffer_Table[o].TexCoord3.y = b;
			g_InlineVertexBuffer_Table[o].TexCoord3.z = c;

            if( (g_InlineVertexBuffer_FVF & D3DFVF_TEXCOUNT_MASK) < D3DFVF_TEX3)
            {
				// Dxbx fix : Use mask, else the format might get expanded incorrectly :
				g_InlineVertexBuffer_FVF = (g_InlineVertexBuffer_FVF & ~D3DFVF_TEXCOUNT_MASK) | D3DFVF_TEX3;
			}

			break;
        }

		case X_D3DVSDE_TEXCOORD3:
        {
            g_InlineVertexBuffer_Table[o].TexCoord4.x = a;
            g_InlineVertexBuffer_Table[o].TexCoord4.y = b;
			g_InlineVertexBuffer_Table[o].TexCoord4.z = c;

            if( (g_InlineVertexBuffer_FVF & D3DFVF_TEXCOUNT_MASK) < D3DFVF_TEX4)
            {
				// Dxbx fix : Use mask, else the format might get expanded incorrectly :
                g_InlineVertexBuffer_FVF = (g_InlineVertexBuffer_FVF & ~D3DFVF_TEXCOUNT_MASK) | D3DFVF_TEX4;
            }

	        break;
        }

        default:
            CxbxKrnlCleanup("Unknown IVB Register : %d", Register);
    }   

	// Is this a write to D3DVSDE_VERTEX?
	if (Register == X_D3DVSDE_VERTEX) {
		// Start a new vertex
		g_InlineVertexBuffer_TableOffset++;
		// Copy all attributes of the previous vertex (if any) to the new vertex
		static UINT uiPreviousOffset = ~0; // Can't use 0, as that may be copied too
		if (uiPreviousOffset != ~0) {
			g_InlineVertexBuffer_Table[g_InlineVertexBuffer_TableOffset] = g_InlineVertexBuffer_Table[uiPreviousOffset];
		}
		uiPreviousOffset = g_InlineVertexBuffer_TableOffset;
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexData4ub)
(
	INT		Register,
	BYTE	a,
	BYTE	b,
	BYTE	c,
	BYTE	d
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexData4f");

	DWORD dwA = a, dwB = b, dwC = c, dwD = d;

    EMUPATCH(D3DDevice_SetVertexData4f)(Register, DWtoF(dwA), DWtoF(dwB), DWtoF(dwC), DWtoF(dwD));
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexData4s)
(
	INT		Register,
	SHORT	a,
	SHORT	b,
	SHORT	c,
	SHORT	d
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexData4f");

	DWORD dwA = a, dwB = b, dwC = c, dwD = d;

    EMUPATCH(D3DDevice_SetVertexData4f)(Register, DWtoF(dwA), DWtoF(dwB), DWtoF(dwC), DWtoF(dwD));
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexDataColor)
(
    int         Register,
    D3DCOLOR    Color
)
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetVertexData4f");

    FLOAT a = DWtoF((Color & 0xFF000000) >> 24);
    FLOAT r = DWtoF((Color & 0x00FF0000) >> 16);
    FLOAT g = DWtoF((Color & 0x0000FF00) >> 8);
    FLOAT b = DWtoF((Color & 0x000000FF) >> 0);

    EMUPATCH(D3DDevice_SetVertexData4f)(Register, r, g, b, a);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_End)()
{
	FUNC_EXPORTS

	LOG_FUNC();

    if(g_InlineVertexBuffer_TableOffset > 0)
        EmuFlushIVB();

    // TODO: Should technically clean this up at some point..but on XP doesnt matter much
//    g_MemoryManager.Free(g_InlineVertexBuffer_pData);
//    g_MemoryManager.Free(g_InlineVertexBuffer_Table);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_RunPushBuffer)
(
    X_D3DPushBuffer       *pPushBuffer,
    X_D3DFixup            *pFixup
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pPushBuffer)
		LOG_FUNC_ARG(pFixup)
		LOG_FUNC_END;

	CxbxUpdateNativeD3DResources();

	EmuExecutePushBuffer(pPushBuffer, pFixup);    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_Clear)
(
    DWORD           Count,
    CONST D3DRECT  *pRects,
    DWORD           Flags,
    D3DCOLOR        Color,
    float           Z,
    DWORD           Stencil
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Count)
		LOG_FUNC_ARG(pRects)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_ARG(Color)
		LOG_FUNC_ARG(Z)
		LOG_FUNC_ARG(Stencil)
		LOG_FUNC_END;

	CxbxClear(Count, pRects, Flags, Color, Z, Stencil);
}

#define CXBX_SWAP_PRESENT_FORWARD (256 + 4 + 1) // = CxbxPresentForwardMarker + D3DSWAP_FINISH + D3DSWAP_COPY

VOID WINAPI XTL::EMUPATCH(D3DDevice_Present)
(
    CONST RECT* pSourceRect,
    CONST RECT* pDestRect,
    PVOID       pDummy1,
    PVOID       pDummy2
)
{
	FUNC_EXPORTS

	// LOG_FORWARD("D3DDevice_Swap");
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pSourceRect)
		LOG_FUNC_ARG(pDestRect)
		LOG_FUNC_ARG(pDummy1)
		LOG_FUNC_ARG(pDummy2)
		LOG_FUNC_END;

	EMUPATCH(D3DDevice_Swap)(CXBX_SWAP_PRESENT_FORWARD); // Xbox present ignores
}

DWORD WINAPI XTL::EMUPATCH(D3DDevice_Swap)
(
    DWORD Flags
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Flags);

	static clock_t lastDrawFunctionCallTime = 0;

    // TODO: Ensure this flag is always the same across library versions
    if(Flags != 0)
		if (Flags != CXBX_SWAP_PRESENT_FORWARD) // Avoid a warning when forwarded
			EmuWarning("XTL::EmuD3DDevice_Swap: Flags != 0");

	// TODO: Make a video option to wait for VBlank before calling Present.
	// Makes syncing to 30fps easier (which is the native frame rate for Azurik
	// and Halo).
//	g_pDD7->WaitForVerticalBlank( DDWAITVB_BLOCKEND, NULL );
//	g_pDD7->WaitForVerticalBlank( DDWAITVB_BLOCKEND, NULL );

	clock_t currentDrawFunctionCallTime = clock();

	CxbxPresent();

	g_DeltaTime += currentDrawFunctionCallTime - lastDrawFunctionCallTime;
	lastDrawFunctionCallTime = currentDrawFunctionCallTime;
	g_Frames++;

	if (g_DeltaTime >= CLOCKS_PER_SEC) {
		UpdateCurrentMSpFAndFPS();
		g_Frames = 0;
		g_DeltaTime -= CLOCKS_PER_SEC;
	}
	
	if (Flags == CXBX_SWAP_PRESENT_FORWARD) // Only do this when forwarded from Present
	{
		// Put primitives per frame in the title
		/*{
			char szString[64];

			sprintf( szString, "Cxbx: PPF(%d)", g_dwPrimPerFrame );

			SetWindowText( CxbxKrnl_hEmuParent, szString );

			g_dwPrimPerFrame = 0;
		}*/

		// TODO : Check if this should be done at Swap-not-Present-time too :
		// not really accurate because you definately dont always present on every vblank
		g_VBData.SwapCounter = g_VBData.VBlankCounter;

		if (g_VBData.VBlankCounter == g_VBLastSwap + 1)
			g_VBData.Flags = 1; // D3DVBLANK_SWAPDONE
		else
		{
			g_VBData.Flags = 2; // D3DVBLANK_SWAPMISSED
			g_SwapData.MissedVBlanks++;
		}
	}

	// Handle Swap Callback function
	{
		g_SwapData.Swap++;

		if(g_pSwapCallback != NULL) 
		{
				
			g_pSwapCallback(&g_SwapData);
				
		}
	}

	g_bHackUpdateSoftwareOverlay = FALSE;

	DWORD result;
	if (Flags == CXBX_SWAP_PRESENT_FORWARD) // Only do this when forwarded from Present
		result = D3D_OK; // Present always returns success
	else
		result = g_SwapData.Swap; // Swap returns number of swaps

    return result;
}

// TODO : Move to own file
struct ConvertedVertexBuffer {
	DWORD Hash = 0;
	//UINT XboxDataSize = 0;
	// DWORD XboxDataSamples[16] = {}; // Read sample indices using https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs
	XTL::IDirect3DVertexBuffer8* pConvertedHostVertexBuffer = nullptr;
};

// TODO : Move to own file
XTL::IDirect3DVertexBuffer8 *XTL::CxbxUpdateVertexBuffer
(
	const XTL::X_D3DVertexBuffer  *pXboxVertexBuffer
)
{
	static std::map<xbaddr, ConvertedVertexBuffer> g_ConvertedVertexBuffers;

	LOG_INIT // Allows use of DEBUG_D3DRESULT

	if (pXboxVertexBuffer == NULL)
		return nullptr;

	xbaddr pVertexBufferData = (xbaddr)GetDataFromXboxResource((X_D3DResource *)pXboxVertexBuffer);
	if (pVertexBufferData == NULL)
		return nullptr; // TODO : Cleanup without data?

	int Size = g_MemoryManager.QueryAllocationSize((void *)pVertexBufferData); // TODO : Use g_MemoryManager.QueryRemainingAllocationSize

	// TODO : Don't hash every time (peek at how the vertex buffer cache avoids this)
	uint32_t uiHash = 0; // TODO : seed with characteristics
	uiHash = XXHash32::hash((void *)pVertexBufferData, (uint64_t)Size, uiHash);

	// TODO : Lock all access to g_ConvertedVertexBuffers

	// Poor-mans cache-eviction : Clear when full.
	if (g_ConvertedVertexBuffers.size() >= MAX_CACHE_SIZE_VERTEXBUFFERS) {
		DbgPrintf("Vertex buffer cache full - clearing and repopulating");
		for (auto it = g_ConvertedVertexBuffers.begin(); it != g_ConvertedVertexBuffers.end(); ++it) {
			auto pHostVertexBuffer = it->second.pConvertedHostVertexBuffer;
			if (pHostVertexBuffer != nullptr) {
				pHostVertexBuffer->Release(); // avoid memory leaks
			}
		}

		g_ConvertedVertexBuffers.clear();
	}

	// Reference the converted vertex buffer (when it's not present, it's added) :
	ConvertedVertexBuffer &convertedVertexBuffer = g_ConvertedVertexBuffers[pVertexBufferData];

	// Check if the data needs an updated conversion or not
	IDirect3DVertexBuffer8 *result = convertedVertexBuffer.pConvertedHostVertexBuffer;
	if (result != nullptr)
	{
		if (uiHash == convertedVertexBuffer.Hash)
			// Hash is still the same - assume the converted resource doesn't require updating
			// TODO : Maybe, if the converted resource gets too old, an update might still be wise
			// to cater for differences that didn't cause a hash-difference (slight chance, but still).s
			return result;

		convertedVertexBuffer = {};
		result->Release();
		result = nullptr;
	}

	convertedVertexBuffer.Hash = uiHash;

	HRESULT hRet = D3D_OK;

    DbgPrintf("CxbxUpdateVertexBuffer : Creating VertexBuffer...\n");

	IDirect3DVertexBuffer8  *pNewHostVertexBuffer = nullptr;

    // create vertex buffer
    hRet = g_pD3DDevice8->CreateVertexBuffer
    (
        Size, 0/*D3DUSAGE_WRITEONLY?*/, 0, D3DPOOL_MANAGED,
        &pNewHostVertexBuffer
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexBuffer");

	if(FAILED(hRet))
		CxbxKrnlCleanup("CreateVertexBuffer Failed!\n\nVB Size = 0x%X\n\nError: \nDesc: ", Size/*,
			DXGetErrorString8A(hRet)*//*, DXGetErrorDescription8A(hRet)*/);

    #ifdef _DEBUG_TRACK_VB
    g_VBTrackTotal.insert(pNewHostVertexBuffer);
    #endif

    BYTE *pNativeData = nullptr;

    hRet = pNewHostVertexBuffer->Lock(
		/*OffsetToLock=*/0, 
		/*SizeToLock=*/0/*=entire buffer*/, 
		&pNativeData, 
		/*Flags=*/D3DLOCK_DISCARD);
	DEBUG_D3DRESULT(hRet, "pNewHostVertexBuffer->Lock");

	if(FAILED(hRet))
        CxbxKrnlCleanup("VertexBuffer Lock Failed!\n\nError: \nDesc: "/*,
			DXGetErrorString8A(hRet)*//*, DXGetErrorDescription8A(hRet)*/);

	// TODO : Apply CxbxVertexBufferConverter here once

    memcpy(pNativeData, (void*)pVertexBufferData, Size);
    pNewHostVertexBuffer->Unlock();

	//SetHostVertexBuffer((XTL::X_D3DResource *)pXboxVertexBuffer, pNewHostVertexBuffer);

	result = pNewHostVertexBuffer;
	result->AddRef();
	convertedVertexBuffer.pConvertedHostVertexBuffer = result;

    DbgPrintf("CxbxUpdateVertexBuffer : Successfully Created VertexBuffer (0x%.08X)\n", result);

    return result;
}

// TODO : Move to own file
XTL::IDirect3DBaseTexture8 *XTL::CxbxUpdateTexture
(
	XTL::X_D3DPixelContainer *pPixelContainer,
	const DWORD *pPalette
)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	if (pPixelContainer == NULL)
		return nullptr;

#ifndef USE_XBOX_BUFFERS
	if (pPixelContainer == g_pInitialXboxBackBuffer)
		return (IDirect3DBaseTexture8 *)g_pInitialHostBackBuffer;

	if (pPixelContainer == g_pInitialXboxRenderTarget)
		return (IDirect3DBaseTexture8 *)g_pInitialHostRenderTarget;

	if (pPixelContainer == g_pInitialXboxDepthStencil)
		return (IDirect3DBaseTexture8 *)g_pInitialHostDepthStencil;

	if (pPixelContainer == g_pActiveXboxBackBuffer)
		if (g_pActiveHostBackBuffer != nullptr)
			return (IDirect3DBaseTexture8 *)g_pActiveHostBackBuffer;

	if (pPixelContainer == g_pActiveXboxRenderTarget)
		if (g_pActiveHostRenderTarget != nullptr)
			return (IDirect3DBaseTexture8 *)g_pActiveHostRenderTarget;

	if (pPixelContainer == g_pActiveXboxDepthStencil)
		if (g_pActiveHostDepthStencil != nullptr)
			return (IDirect3DBaseTexture8 *)g_pActiveHostDepthStencil;
#endif

	X_D3DFORMAT X_Format = GetXboxPixelContainerFormat(pPixelContainer);
	if (X_Format == X_D3DFMT_P8)
		if (pPalette == NULL) {
			// Test case : aerox2
			EmuWarning("CxbxUpdateTexture can't handle paletized textures without a given palette!");
			return nullptr;
		}

#if 0
	if (IsSpecialXboxResource(pPixelContainer))
		return nullptr;
#endif

	PVOID pTextureData = GetDataFromXboxResource((X_D3DResource *)pPixelContainer);
	if (pTextureData == NULL)
		return nullptr; // TODO : Cleanup without data?

	// Construct the identifying key for this Xbox texture
	const struct TextureCache::TextureResourceKey textureKey = { (xbaddr)pTextureData, 0, 0 }; // Expiriment : ignore Format and Size
//	const struct TextureCache::TextureResourceKey textureKey = { (xbaddr)pTextureData, pPixelContainer->Format, pPixelContainer->Size };

	// Find a cached host texture
	struct TextureCache::TextureCacheEntry &CacheEntry = g_TextureCache.Find(textureKey, pPalette);
	if (CacheEntry.pConvertedHostTexture != nullptr)
		return CacheEntry.pConvertedHostTexture;

#if 0 // unused
	// Make sure D3DDevice_SwitchTexture can associate a Data pointer with this texture
	g_DataToTexture.insert(pTextureData, (void *)pPixelContainer);
#endif

	IDirect3DBaseTexture8 *result = nullptr;

	// Interpret Width/Height/BPP
	DecodedPixelContainer PixelJar;
	DecodeD3DFormatAndSize(pPixelContainer, OUT PixelJar);

#if 0 // TODO : Why was this?
	if (PixelJar.bIsSwizzled || PixelJar.bIsCompressed)
	{
		uint32 w = PixelJar.dwWidth;
		uint32 h = PixelJar.dwHeight;

		for (uint32 v = 0; v<PixelJar.dwMipMapLevels; v++)
		{
			if (((1u << v) >= w) || ((1u << v) >= h))
			{
				PixelJar.dwMipMapLevels = v + 1;
				break;
			}
		}
	}
#endif

	// TODO: HACK: Temporary?
	switch (X_Format)
	{
// TODO :  Disable to see if DepthStencils would break anything
	case X_D3DFMT_LIN_D24S8:
	{
		// Test case : Turok hits this
		EmuWarning("D3DFMT_LIN_D24S8 not yet supported!");
		X_Format = X_D3DFMT_LIN_A8R8G8B8;
		break;
	}
	case X_D3DFMT_LIN_D16:
	{
		// Test case : aerox 2 hits this
		EmuWarning("D3DFMT_LIN_D16 not yet supported!");
		X_Format = X_D3DFMT_LIN_R5G6B5;
		break;
	}
	case X_D3DFMT_X1R5G5B5:
	{
		// TODO: HACK : (blueshogun) Since I have trouble with this texture format on modern hardware,
		// Let's try using some 16-bit format instead...
		EmuWarning("X_D3DFMT_X1R5G5B5 -> D3DFMT_R5GB5");
		X_Format = X_D3DFMT_R5G6B5;
		break;
	}
	case X_D3DFMT_UYVY:
	case X_D3DFMT_YUY2:
	{
		if (GetXboxRenderState(X_D3DRS_YUVENABLE) == (DWORD)TRUE)
		{
#if 0
			if (X_Format == X_D3DFMT_YUY2)
			{
				// cache the overlay size
				g_dwOverlayW = dwWidth;
				g_dwOverlayH = dwHeight;
				g_dwOverlayP = RoundUp(g_dwOverlayW, 64) * dwBPP / 8;
			}
#endif

			break;
		}
		else
		{
			EmuWarning("RenderState D3DRS_YUVENABLE not set for Y-Cb-Cr. Colors will be wrong");
			// TODO : To handle this like the NV2A does, we should decompress the
			// Y, Cb and Cr channels per pixel, but leave out the conversion to RGB,
			// but instead show the values themselves as RGB values (use Cr for Red,
			// Y for Green, and Cb for Blue).
			X_Format = X_D3DFMT_LIN_R5G6B5; // HACK to at least keep the BitsPerPixel compatible
		}
	}
	}

	// One of these will be created :
	IDirect3DSurface8 *pNewHostSurface = nullptr;
	IDirect3DTexture8 *pNewHostTexture = nullptr;
	IDirect3DCubeTexture8 *pNewHostCubeTexture = nullptr;
	IDirect3DVolumeTexture8 *pNewHostVolumeTexture = nullptr;

	// Note : For now, we create and fill textures only once (Xbox changes are put
	// in new host resources), so it seems using D3DUSAGE_DYNAMIC and D3DPOOL_DEFAULT
	// isn't needed (plus, we don't have to check for D3DCAPS2_DYNAMICTEXTURES)
	DWORD D3DUsage = 0; // TODO : | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL
	D3DPOOL D3DPool = D3DPOOL_MANAGED;

	X_D3DRESOURCETYPE X_D3DResourceType = GetXboxD3DResourceType(pPixelContainer);

	// Determine if a conversion to ARGB is needed
	bool bConvertToARGB = false;
	D3DFORMAT PCFormat = DxbxXB2PC_D3DFormat(X_Format, X_D3DResourceType, D3DUsage, bConvertToARGB);

	HRESULT hRet = S_OK;

	// create the happy little texture
	DWORD dwCommonType = GetXboxCommonResourceType(pPixelContainer);
	// TODO : Remove by splitting this over texture and surface variants
	if (dwCommonType == X_D3DCOMMON_TYPE_SURFACE) // X_D3DRTYPE_SURFACE
	{
		X_D3DBaseTexture *pSurfaceParent = ((X_D3DSurface*)pPixelContainer)->Parent;
		// Retrieve the PC resource that represents the parent of this surface,
		// including it's contents (updated if necessary) :
		// Samples like CubeMap show that render target formats must be applied to both surface and texture:
		IDirect3DBaseTexture8 *pHostParentTexture = CxbxUpdateTexture(pSurfaceParent, pPalette);
		if (pHostParentTexture != nullptr) {
			// Determine which face & mipmap level where used in the creation of this Xbox surface, using the Data pointer :
			UINT Level;
			D3DCUBEMAP_FACES FaceType;
			DxbxDetermineSurFaceAndLevelByData(PixelJar, /*OUT*/Level, /*OUT*/FaceType);
			switch (GetXboxD3DResourceType(pSurfaceParent)) {
			case X_D3DRTYPE_TEXTURE: {
				LOG_TEST_CASE("pSurfaceParent X_D3DRTYPE_TEXTURE");
				hRet = ((IDirect3DTexture8*)pHostParentTexture)->GetSurfaceLevel(
					Level,
					&pNewHostSurface);
				DEBUG_D3DRESULT(hRet, "pHostParentTexture->GetSurfaceLevel");
				break;
			}
			case X_D3DRTYPE_CUBETEXTURE: {
				LOG_TEST_CASE("pSurfaceParent X_D3DRTYPE_CUBETEXTURE");
				hRet = ((IDirect3DCubeTexture8*)pHostParentTexture)->GetCubeMapSurface(
					FaceType,
					Level,
					&pNewHostSurface);
				DEBUG_D3DRESULT(hRet, "pHostParentTexture->GetCubeMapSurface");
				// PS: It's probably needed to patch up the destruction of this surface too!
				// PS2: We also need a mechanism to remove obsolete native resources,
				// like the native CubeMap texture that's used for this surfaces' parent.
				break;
			}
			default:
				CxbxKrnlCleanup("pSurfaceParent Unhandled type");
				;//DxbxD3DError("DxbxAssureNativeResource", "Unhandled D3DSurface.Parent type!", pPixelContainer);
			}
		}
		else {
			// Differentiate between images & depth buffers :
			if (EmuXBFormatIsRenderTarget(X_Format)) {
				hRet = g_pD3DDevice8->CreateImageSurface(PixelJar.dwWidth, PixelJar.dwHeight, PCFormat, &pNewHostSurface);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateImageSurface");

				if (FAILED(hRet))
					CxbxKrnlCleanup("CreateImageSurface Failed!\n\nError: %s\nDesc: %s"/*,
					DXGetErrorString8A(hRet), DXGetErrorDescription8A(hRet)*/);
			}
			else {
				if (EmuXBFormatIsDepthBuffer(X_Format)) {
					LOG_TEST_CASE("pSurfaceParent IsDepthBuffer");
					hRet = g_pD3DDevice8->CreateDepthStencilSurface(
						PixelJar.dwWidth, PixelJar.dwHeight,
						PCFormat,
						D3DMULTISAMPLE_NONE, // MultiSampleType; TODO : Determine real MultiSampleType
						&pNewHostSurface);
					DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateDepthStencilSurface");

					if (FAILED(hRet))
						CxbxKrnlCleanup("CreateDepthStencilSurface Failed!\n\nError: %s\nDesc: %s"/*,
						DXGetErrorString8A(hRet), DXGetErrorDescription8A(hRet)*/);
				}
				else
					CxbxKrnlCleanup("pSurfaceParent Unhandled format");
			}
		}

		//SetHostSurface((X_D3DResource *)pPixelContainer, pNewHostSurface);
		DbgPrintf("CxbxUpdateTexture : Successfully created surface (0x%.08X, 0x%.08X)\n", pPixelContainer, pNewHostSurface);
		DbgPrintf("CxbxUpdateTexture : Width : %d, Height : %d, Format : %d\n", PixelJar.dwWidth, PixelJar.dwHeight, PCFormat);

		result = (IDirect3DBaseTexture8 *)pNewHostSurface;
	}
	else
	{
#if 0
		// TODO: HACK: Figure out why this is necessary!
		// TODO: This is necessary for DXT1 textures at least (4x4 blocks minimum)
		if (dwWidth < 4)
		{
			EmuWarning("Expanding texture width (%d->4)", dwWidth);
			dwWidth = 4;

			dwMipMapLevels = 3;
		}

		if (dwHeight < 4)
		{
			EmuWarning("Expanding texture height (%d->4)", dwHeight);
			dwHeight = 4;

			dwMipMapLevels = 3;
		}
#endif
		if (PixelJar.format.bIsCubeMap)
		{
			DbgPrintf("CreateCubeTexture(%d, %d, 0, %d, D3DPOOL_MANAGED)\n", 
				PixelJar.dwWidth, PixelJar.dwMipMapLevels, PCFormat);

			hRet = g_pD3DDevice8->CreateCubeTexture
			(
				PixelJar.dwWidth, PixelJar.dwMipMapLevels, D3DUsage, PCFormat,
				D3DPool, &pNewHostCubeTexture
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateCubeTexture");

			if (FAILED(hRet))
				CxbxKrnlCleanup("CreateCubeTexture Failed!\n\nError: \nDesc: "/*,
				DXGetErrorString8A(hRet), DXGetErrorDescription8A(hRet)*/);

			//SetHostCubeTexture((X_D3DResource *)pPixelContainer, pNewHostCubeTexture);
			DbgPrintf("CxbxUpdateTexture : CreateCubeTexture succeeded : 0x%.08X (0x%.08X)\n", pPixelContainer, pNewHostCubeTexture);

			result = (IDirect3DBaseTexture8 *)pNewHostCubeTexture;
		}
		else if (PixelJar.bIs3D)
		{
			// Hit by XDK sample FuzzyTeapot
			DbgPrintf("CreateVolumeTexture(%d, %d, 0, %d, D3DPOOL_MANAGED)\n", 
				PixelJar.dwWidth, PixelJar.dwMipMapLevels, PCFormat);

			hRet = g_pD3DDevice8->CreateVolumeTexture
			(
				PixelJar.dwWidth, PixelJar.dwHeight, PixelJar.dwDepth, PixelJar.dwMipMapLevels,
				D3DUsage,
				PCFormat, D3DPool, &pNewHostVolumeTexture
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVolumeTexture");

			if (FAILED(hRet))
				CxbxKrnlCleanup("CreateVolumeTexture Failed!\n\nError: \nDesc: "/*,
				DXGetErrorString8A(hRet), DXGetErrorDescription8A(hRet)*/);

			//SetHostVolumeTexture((X_D3DResource *)pPixelContainer, pNewHostVolumeTexture);
			DbgPrintf("CxbxUpdateTexture: CreateVolumeTexture succeeded : 0x%.08X (0x%.08X)\n", pPixelContainer, pNewHostVolumeTexture);

			result = (IDirect3DBaseTexture8 *)pNewHostVolumeTexture;
		} else
		{
			// HACK: Quantum Redshift
			/*if( dwMipMapLevels == 8 && X_Format == X_D3DFMT_DXT1 )
			{
				printf( "Dirty Quantum Redshift hack applied!\n" );
				dwMipMapLevels = 1;
			}*/

			hRet = g_pD3DDevice8->CreateTexture
			(
				PixelJar.dwWidth, PixelJar.dwHeight, PixelJar.dwMipMapLevels, D3DUsage, PCFormat,
				D3DPool, &pNewHostTexture
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateTexture");

			/*if(FAILED(hRet))
			{
				hRet = g_pD3DDevice8->CreateTexture
				(
					PixelJar.dwWidth, PixelJar.dwHeight, PixelJar.dwMipMapLevels, D3DUsage, PCFormat,
					D3DPOOL_SYSTEMMEM, &pNewHostTexture
				);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateTexture(D3DPOOL_SYSTEMMEM)");
			}*/

			if(FAILED(hRet))
				EmuWarning("CreateTexture Failed!\n\n"
					"Error: 0x%X\nFormat: %d\nDimensions: %dx%d", hRet, PCFormat, PixelJar.dwWidth, PixelJar.dwHeight);
			else
            {
				//SetHostTexture((X_D3DResource *)pPixelContainer, pNewHostTexture);
				DbgPrintf("CxbxUpdateTexture : CreateTexture succeeded : 0x%.08X (0x%.08X)\n", pPixelContainer, pNewHostTexture);

				result = (IDirect3DBaseTexture8 *)pNewHostTexture;
			}
        }
    }

	/* TODO : // Let DirectX convert the surface (including palette formats) :
	D3DXLoadSurfaceFromMemory(
		result,
		nullptr, // no destination palette
		&destRect,
		pSrc, // Source buffer
		dwMipPitch, // Source pitch
		pPalette,
		&SrcRect,
		D3DX_DEFAULT, // D3DX_FILTER_NONE,
		0 // No ColorKey?
		);
	*/

	BYTE* pTmpBuffer = nullptr;

	// This outer loop walks over all faces (6 for CubeMaps, just 1 for anything else) :
	uint nrfaces = PixelJar.format.bIsCubeMap ? 6 : 1;
	for (uint face = 0; face < nrfaces; face++)
	{
		// as we iterate through mipmap levels, we'll adjust the source resource offset
		DWORD dwMipWidth = PixelJar.dwWidth;
		DWORD dwMipHeight = PixelJar.dwHeight;
		DWORD dwMipPitch = PixelJar.dwRowPitch;

		// This 2nd loop iterates through all mipmap levels :
		for (uint level = 0; level < PixelJar.dwMipMapLevels; level++)
		{
			D3DLOCKED_BOX LockedBox = {};
			DWORD dwMipSizeInBytes = PixelJar.MipMapOffsets[level + 1] - PixelJar.MipMapOffsets[level];
			DWORD D3DLockFlags = D3DLOCK_NOSYSLOCK; // Avoid system-wide lock, we're just creating a resource
			if (D3DUsage & D3DUSAGE_DYNAMIC) // Only allowed on dynamic textures:
				D3DLockFlags |= D3DLOCK_DISCARD; // we'll overwrite the entire resource

			if (pNewHostVolumeTexture != nullptr) {
				hRet = pNewHostVolumeTexture->LockBox(level, &LockedBox, nullptr, D3DLockFlags);
				DEBUG_D3DRESULT(hRet, "pNewHostVolumeTexture->LockBox");
			}
			else {
				D3DLOCKED_RECT LockedRect = {};

				if (pNewHostTexture != nullptr) {
					hRet = pNewHostTexture->LockRect(level, &LockedRect, nullptr, D3DLockFlags);
					DEBUG_D3DRESULT(hRet, "pNewHostTexture->LockRect");
				} else if (pNewHostCubeTexture != nullptr) {
					if (level == 0) { // Expirimental : Cache all 6 sides of a cube, to support render-targets 
						BYTE *pFaceData = (BYTE*)pTextureData;
						pFaceData += PixelJar.dwFacePitch * face;
						const struct TextureCache::TextureResourceKey surfaceKey = { (xbaddr)pFaceData, 0, 0 }; // Expiriment : ignore Format and Size
//						const struct TextureCache::TextureResourceKey surfaceKey = { (xbaddr)pFaceData, pPixelContainer->Format, pPixelContainer->Size };
						struct TextureCache::TextureCacheEntry &SurfaceCacheEntry = g_TextureCache.Find(surfaceKey, pPalette);

						IDirect3DSurface8 *pSurface = nullptr;
						pNewHostCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)face, 0, &pSurface);

						g_TextureCache.AddConvertedResource(SurfaceCacheEntry, (XTL::IDirect3DBaseTexture8*)pSurface);
					}

					hRet = pNewHostCubeTexture->LockRect((D3DCUBEMAP_FACES)face, level, &LockedRect, nullptr, D3DLockFlags);
					DEBUG_D3DRESULT(hRet, "pNewHostCubeTexture->LockRect");
				}
				else {
					assert(pNewHostSurface != nullptr);
					hRet = pNewHostSurface->LockRect(&LockedRect, nullptr, D3DLockFlags);
					DEBUG_D3DRESULT(hRet, "pNewHostSurface->LockRect");
				}

				if (FAILED(hRet)) {
					LOG_TEST_CASE("What should we do when the resource couldn't be locked?");
				}

				// Transfer LockedRect to LockedBox, so following code can work with just that
				LockedBox.pBits = LockedRect.pBits;
				LockedBox.RowPitch = LockedRect.Pitch;
				LockedBox.SlicePitch = 0; // 2D textures don't use slices
			}

			// This inner loop walks over all slices (1 for anything but 3D textures - those
			// use the correct amount for the current mipmap level, but never go below 1) :
			uint nrslices = PixelJar.MipMapSlices[level];
			for (uint slice = 0; slice < nrslices; slice++)
			{
				// Determine the Xbox data pointer and step it to the correct face, mipmap and/or slice :
				BYTE *pSrc = (BYTE*)pTextureData;
				DWORD dwSrcPitch = dwMipPitch;
				// Do the same with the host data pointer
				BYTE *pDest = (BYTE*)LockedBox.pBits;
				DWORD dwDestPitch = LockedBox.RowPitch;

				pSrc += PixelJar.MipMapOffsets[level];
				if (PixelJar.format.bIsCubeMap) {
					pSrc += PixelJar.dwFacePitch * face;
					// pDest is already locked by face, so doesn't need stepping
				}
				else {
					if (PixelJar.bIs3D)
					{
						// Use the Xbox slice pitch (from the current level) to step to each slice in src :
						pSrc += PixelJar.SlicePitches[level] * slice;
						// Use the native SlicePitch (from the locked box) to step to each slice in dest :
						pDest += LockedBox.SlicePitch * slice;
					}
				}

				if (slice == 0) // TODO : Why does Dxbx only unswizzle the first slice?
				{
					// Copy over data (deswizzle if necessary)
					if (PixelJar.bIsSwizzled)
					{
						assert(!PixelJar.bIsCompressed); // compressed format mutually exclusive with swizzled format

						BYTE *pDest2 = pDest; // Remember pDest for later reuse

						if (bConvertToARGB) {
							// If we must both unswizzle AND convert to ARGB, we need an intermediate buffer
							if (pTmpBuffer == nullptr)
								// Allocate buffer once, so loop repeats can re-use it without reallocating.
								// This initial allocation is big enough for all following uses (cube faces/texture mipmap levels/volume slices).
								pTmpBuffer = (BYTE *)malloc(dwMipSizeInBytes);

							// Unswizzle towards that buffer
							pDest = pTmpBuffer;							
							dwDestPitch = dwMipPitch;
						}

						EmuUnswizzleRect
						(
							pSrc, dwMipWidth, dwMipHeight, 1, // Was PixelJar.dwDepth, TODO : Why does this break Turok gallery?
							pDest, dwDestPitch, PixelJar.dwBPP / 8
						);
						CXBX_CHECK_INTEGRITY();

						if (bConvertToARGB) {
							// After unswizzling, convert to ARGB from the intermediate (unswizzled) buffer
							pSrc = pDest;
							dwSrcPitch = dwDestPitch;
							// Towards the locked output buffer
							pDest = (BYTE*)pDest2; // reuse original pDest
							dwDestPitch = LockedBox.RowPitch;
						}
					}
				}

				if (bConvertToARGB)
				{
					assert(!PixelJar.bIsCompressed); // compressed format never requires conversion

					uint dwDestWidthInBytes = dwMipWidth * sizeof(D3DCOLOR);

					if (X_Format == X_D3DFMT_P8)
					{
						EmuWarning("Expanding X_D3DFMT_P8 to D3DFMT_A8R8G8B8");

						// Lookup the colors of the paletted pixels in the current pallette
						// and write the expanded color back to the texture :
						unsigned int x = 0, s = 0, d = 0;
						while (s < dwMipSizeInBytes)
						{
							// Read P8 pixel :
							uint8_t pixel_palette_index = ((uint8_t *)pSrc)[s++];
							// Read the corresponding ARGB from the palette and store it in the new texture :
							((D3DCOLOR *)pDest)[d++] = pPalette[pixel_palette_index];
							// Step to the next pixel, check if we've done one scanline :
							if (++x == dwMipWidth)
							{
								// Step to the start of the next scanline :
								x = 0;
								s += dwSrcPitch - dwMipPitch;
								d += dwDestPitch - dwDestWidthInBytes;
							}
						}
						CXBX_CHECK_INTEGRITY();
					}
					else
					{
						EmuWarning("Unsupported texture format, expanding to D3DFMT_A8R8G8B8");

						// Convert a row at a time, using a libyuv-like callback approach :
						const FormatToARGBRow ConvertRowToARGB = EmuXBFormatComponentConverter(X_Format);
						if (ConvertRowToARGB == nullptr)
							CxbxKrnlCleanup("Unhandled conversion!");

						DWORD SrcRowOff = 0;
						uint8 *pDestRow = (uint8 *)pDest;
						while (SrcRowOff < dwMipSizeInBytes) {
							*(int*)pDestRow = dwDestPitch; // Dirty hack, to avoid an extra parameter to all conversion callbacks
							ConvertRowToARGB(((uint8 *)pSrc) + SrcRowOff, pDestRow, dwMipWidth);
							SrcRowOff += dwSrcPitch;
							pDestRow += dwDestPitch;
							CXBX_CHECK_INTEGRITY();
						}
					}
				}

				if (!PixelJar.bIsSwizzled && !bConvertToARGB)
				{
					DWORD dwMipWidthInBytes;

					if (PixelJar.bIsCompressed) {
						// Turok uses DXT1 in splash screen, DXT3 in menu textures - both work correctly with this :
						dwMipWidthInBytes = (dwMipWidth * PixelJar.dwBPP) / 2;
						// Compressed formats encode 4 lines per row of data, hence this division :
						CxbxPitchedCopy(pDest, pSrc, dwDestPitch, dwSrcPitch, dwMipWidthInBytes, dwMipHeight / 4);
					}
					else {
						dwMipWidthInBytes = (dwMipWidth * PixelJar.dwBPP) / 8;
						CxbxPitchedCopy(pDest, pSrc, dwDestPitch, dwSrcPitch, dwMipWidthInBytes, dwMipHeight);
					}
				}

				if (PixelJar.format.bIsBorderTexture)
				{
					if (face == 0 && level == 0 && slice == 0) { // Log warning only once per resource
						EmuWarning("Ignoring D3DFORMAT_BORDERSOURCE_COLOR");
						// LOG_TEST_CASE Test case : Fuzion Frenzy
					}

					// TODO: Emulate X_D3DFORMAT_BORDERSOURCE_COLOR (allows a border to be drawn maybe hack this in software?)
				}
			}

			if (pNewHostTexture != nullptr) {
				hRet = pNewHostTexture->UnlockRect(level);
				DEBUG_D3DRESULT(hRet, "pNewHostTexture->UnlockRect");
			}
			else if (pNewHostCubeTexture != nullptr) {
				hRet = pNewHostCubeTexture->UnlockRect((D3DCUBEMAP_FACES)face, level);
				DEBUG_D3DRESULT(hRet, "pNewHostCubeTexture->UnlockRect");
			}
			else if (pNewHostVolumeTexture != nullptr) {
				hRet = pNewHostVolumeTexture->UnlockBox(level);
				DEBUG_D3DRESULT(hRet, "pNewHostVolumeTexture->UnlockBox");
			}
			else {
				assert(pNewHostSurface != nullptr);
				hRet = pNewHostSurface->UnlockRect();
				DEBUG_D3DRESULT(hRet, "pNewHostSurface->UnlockRect");
			}

			// Step to next mipmap level (but never go below minimum) :
			if (dwMipWidth > PixelJar.dwMinXYValue) {
				dwMipWidth /= 2;
				dwMipPitch /= 2;
			}
			
			if (dwMipHeight > PixelJar.dwMinXYValue)
				dwMipHeight /= 2;
		} // for level
	} // for face

	// Flush temporary data buffer
	if (pTmpBuffer != nullptr)
		free(pTmpBuffer);

	// Debug Texture Dumping
//#define _DEBUG_DUMP_TEXTURE_REGISTER 1
#ifdef _DEBUG_DUMP_TEXTURE_REGISTER
	const D3DXIMAGE_FILEFORMAT DestFormat = D3DXIFF_DDS; // TODO : D3DXIFF_PNG;
	const char * Extension = "dds"; // TODO : "png";
	char szBuffer[255];

	if (dwCommonType == X_D3DCOMMON_TYPE_SURFACE) {
		static int dwDumpSurface = 0;

		sprintf(szBuffer, "%.03d-RegSurface%.03d.%s", X_Format, dwDumpSurface++, Extension);
		D3DXSaveSurfaceToFile(szBuffer, DestFormat, pNewHostSurface, NULL, NULL);
	}
	else {
		if (PixelJar.format.bIsCubeMap) {
			static int dwDumpCube = 0;

			for (int v = 0; v<6; v++) {
				IDirect3DSurface8 *pSurface = nullptr;

				sprintf(szBuffer, "%.03d-RegCubeTex%.03d-%d.%s", X_Format, dwDumpCube++, v, Extension);
				pNewHostCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)v, 0, &pSurface);
				D3DXSaveSurfaceToFile(szBuffer, DestFormat, pSurface, NULL, NULL);
				pSurface->Release();
			}
		}
		else {
			static int dwDumpTex = 0;

			sprintf(szBuffer, "%.03d-RegTexture%.03d.%s", X_Format, dwDumpTex++, Extension);
			D3DXSaveTextureToFile(szBuffer, DestFormat, result, NULL);
		}
	}
#endif

	g_TextureCache.AddConvertedResource(CacheEntry, result);

	return result;
}

#if 0 // Patch disabled
ULONG WINAPI XTL::EMUPATCH(D3DResource_AddRef)
(
    X_D3DResource      *pThis
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pThis);

    if (!pThis)
	{
        EmuWarning("IDirect3DResource8::AddRef() was not passed a valid pointer!");
		return 0;
    }

	// Initially, increment the Xbox refcount and return that
	ULONG uRet = (++(pThis->Common)) & X_D3DCOMMON_REFCOUNT_MASK;

	// Index buffers don't have a native resource assigned
	if (GetXboxCommonResourceType(pThis) != X_D3DCOMMON_TYPE_INDEXBUFFER) {
		CxbxUpdateResource(pThis);

		// If this is the first reference on a surface
		if (uRet == 1)
			if (GetXboxCommonResourceType(pThis) == X_D3DCOMMON_TYPE_SURFACE)
				// Try to AddRef the parent too
				if (((X_D3DSurface *)pThis)->Parent != NULL)
					((X_D3DSurface *)pThis)->Parent->Common++;

		// Try to retrieve the host resource behind this resource
		IDirect3DResource8 *pHostResource = GetHostResource(pThis);
		if (pHostResource != 0)
			// if there's a host resource, AddRef it too and return that
			uRet = pHostResource->AddRef();
	}

	
    return uRet;
}
#endif

#if 0
ULONG WINAPI XTL::EMUPATCH(D3DResource_Release)
(
    X_D3DResource      *pThis
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pThis);

	ULONG uRet;

	if(pThis == NULL)
	{
		// HACK: In case the clone technique fails...
		EmuWarning("NULL texture!");
		uRet = 0;
	}
	else
	{
		// HACK: Disable overlay when a YUV surface is released (regardless which refcount, apparently)
		if (IsYuvSurface(pThis))
			EMUPATCH(D3DDevice_EnableOverlay)(FALSE);

		uRet = (--(pThis->Common)) & X_D3DCOMMON_REFCOUNT_MASK;
		if (uRet == 0)
		{
			IDirect3DResource8 *pHostResource = GetHostResource(pThis);

			if(g_pIndexBuffer == pThis)
				g_pIndexBuffer = NULL;

			if (g_pActiveXboxRenderTarget == pThis)
				g_pActiveXboxRenderTarget = NULL;

			if (g_pActiveXboxDepthStencil == pThis)
				g_pActiveXboxDepthStencil = NULL;

			if (g_pCachedYuvSurface == pThis)
				g_pCachedYuvSurface = NULL;


			if (pHostResource != nullptr)
			{
#ifdef _DEBUG_TRACE_VB
				D3DRESOURCETYPE Type = pHostResource->GetType();
#endif

				/*
				 * Temporarily disable this until we figure out correct reference counting!
				uRet = pHostResource->Release();
				if(uRet == 0 && pThis->Common)
				{
					DbgPrintf("EmuIDirect3DResource8_Release : Cleaned up a Resource!\n");

					#ifdef _DEBUG_TRACE_VB
					if(Type == D3DRTYPE_VERTEXBUFFER)
					{
						g_VBTrackTotal.remove(pHostResource);
						g_VBTrackDisable.remove(pHostResource);
					}
					#endif

					//delete pThis;
				} */
			}

			if (IsXboxResourceD3DCreated(pThis))
			{
				void *XboxData = GetDataFromXboxResource(pThis);

				if (XboxData != NULL)
					if (g_MemoryManager.IsAllocated(XboxData)) // Prevents problems for now (don't free partial or unallocated memory)
						; // g_MemoryManager.Free(XboxData); // TODO : This crashes on "MemoryManager attempted to free only a part of a block"
			}
		}
    }

    return uRet;
}
#endif

BOOL WINAPI XTL::EMUPATCH(D3DResource_IsBusy)
(
    X_D3DResource      *pThis
)
{
	FUNC_EXPORTS

    /* too much output
	LOG_FUNC_ONE_ARG(pThis);
    //*/

    return FALSE;
}

#if 0 // Patch disabled
XTL::X_D3DRESOURCETYPE WINAPI XTL::EMUPATCH(D3DResource_GetType)
(
    X_D3DResource      *pThis
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pThis);

	D3DRESOURCETYPE rType;

	EmuVerifyResourceIsRegistered(pThis);

	// Check for Xbox specific resources (Azurik may need this)
	DWORD dwType = GetXboxCommonResourceType(pThis);

	switch(dwType)
	{
	case X_D3DCOMMON_TYPE_PUSHBUFFER:
		rType = (D3DRESOURCETYPE) 8; break;
	case X_D3DCOMMON_TYPE_PALETTE:
		rType = (D3DRESOURCETYPE) 9; break;
	case X_D3DCOMMON_TYPE_FIXUP:
		rType = (D3DRESOURCETYPE) 10; break;
	default:
		rType = GetHostResource(pThis)->GetType(); break;
	}

    

    return (X_D3DRESOURCETYPE)rType;
}
#endif

#if 0 // Patch disabled - just calls LockSurface (and BlockOnResource)
VOID WINAPI XTL::EMUPATCH(Lock2DSurface)
(
    X_D3DPixelContainer *pPixelContainer,
    D3DCUBEMAP_FACES     FaceType,
    UINT                 Level,
    D3DLOCKED_RECT      *pLockedRect,
    RECT                *pRect,
    DWORD                Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pPixelContainer)
		LOG_FUNC_ARG(FaceType)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pLockedRect)
		LOG_FUNC_ARG(pRect)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    CxbxUpdateResource(pPixelContainer);

	IDirect3DCubeTexture8 *pHostCubeTexture = GetHostCubeTexture(pPixelContainer);

	if (pHostCubeTexture != nullptr)
	{
		HRESULT hRet;

		hRet = pHostCubeTexture->UnlockRect(FaceType, Level); // remove old lock
		DEBUG_D3DRESULT(hRet, "pHostCubeTexture->UnlockRect");

		hRet = pHostCubeTexture->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
		DEBUG_D3DRESULT(hRet, "pHostCubeTexture->LockRect");
	}
}
#endif

#if 0 // Patch disabled - just calls LockSurface (and BlockOnResource)
VOID WINAPI XTL::EMUPATCH(Lock3DSurface)
(
    X_D3DPixelContainer *pPixelContainer,
    UINT				Level,
	D3DLOCKED_BOX		*pLockedVolume,
	D3DBOX				*pBox,
	DWORD				Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pPixelContainer)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pLockedVolume)
		LOG_FUNC_ARG(pBox)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    CxbxUpdateResource(pPixelContainer);

	IDirect3DVolumeTexture8 *pHostVolumeTexture = GetHostVolumeTexture(pPixelContainer);

	if (pHostVolumeTexture != nullptr)
	{
		HRESULT hRet;

		hRet = pHostVolumeTexture->UnlockBox(Level); // remove old lock
		DEBUG_D3DRESULT(hRet, "pHostVolumeTexture->UnlockBox");

		hRet = pHostVolumeTexture->LockBox(Level, pLockedVolume, pBox, Flags);
		DEBUG_D3DRESULT(hRet, "pHostVolumeTexture->LockBox");
	}
}
#endif

#if 0 // Patch disabled now CreateDevice is unpatched (because this reads Data from the first Xbox FrameBuffer)
// Dxbx : Needs a patch because it accesses _D3D__pDevice at some offset,
// probably comparing the data of this pixelcontainer to the framebuffer
// and setting the MultiSampleType as a result to either the device's
// MultiSampleType or D3DMULTISAMPLE_NONE.
VOID WINAPI XTL::EMUPATCH(Get2DSurfaceDesc)
(
	X_D3DPixelContainer *pPixelContainer,
	DWORD                dwLevel,
    X_D3DSURFACE_DESC   *pDesc
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pPixelContainer)
		LOG_FUNC_ARG(dwLevel)
		LOG_FUNC_ARG(pDesc)
		LOG_FUNC_END;

	DecodedPixelContainer PixelJar;
	DecodeD3DFormatAndSize(pPixelContainer, OUT PixelJar);

	// TODO : Check if IsYuvSurface(pPixelContainer) works too
	pDesc->Format = PixelJar.format.X_Format;
    pDesc->Type = GetXboxD3DResourceType(pPixelContainer);
	
	// Include X_D3DUSAGE_RENDERTARGET or X_D3DUSAGE_DEPTHSTENCIL where appropriate, which means :
	// only at dwLevel=0, when this is actually the active render target or depth stencil buffer.
	pDesc->Usage = 0;
	if (dwLevel == 0)
	{
		if (pPixelContainer = g_pActiveXboxRenderTarget)
			pDesc->Usage = X_D3DUSAGE_RENDERTARGET;
		else
			if (pPixelContainer = g_pActiveXboxDepthStencil)
				pDesc->Usage = X_D3DUSAGE_DEPTHSTENCIL;
	}

	pDesc->Size = PixelJar.MipMapOffsets[dwLevel + 1] - PixelJar.MipMapOffsets[dwLevel];
	pDesc->MultiSampleType = X_D3DMULTISAMPLE_NONE; // TODO : If this is the active backbuffer, use the devices' multi sampling
	pDesc->Width = PixelJar.dwWidth >> dwLevel; // ??
	pDesc->Height = PixelJar.dwHeight >> dwLevel; // ??
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DSurface_GetDesc)
(
    X_D3DResource      *pThis,
    X_D3DSURFACE_DESC  *pDesc
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(pDesc)
		LOG_FUNC_END;

    HRESULT hRet;

    EmuVerifyResourceIsRegistered(pThis);

    if(IsYuvSurface(pThis))
    {
        pDesc->Format = X_D3DFMT_YUY2;
        pDesc->Height = g_dwOverlayH;
        pDesc->Width  = g_dwOverlayW;
        pDesc->MultiSampleType = (D3DMULTISAMPLE_TYPE)0;
        pDesc->Size   = g_dwOverlayP*g_dwOverlayH;
        pDesc->Type   = X_D3DRTYPE_SURFACE;
        pDesc->Usage  = 0;

        hRet = D3D_OK;
    }
    else
    {
        IDirect3DSurface8 *pHostSurface = GetHostSurface(pThis);

        D3DSURFACE_DESC SurfaceDesc;

		if( pHostSurface != nullptr )
		{
			// rearrange into windows format (remove D3DPool)
			hRet = pHostSurface->GetDesc(&SurfaceDesc);

			if (SUCCEEDED(hRet))
			{
				// Convert Format (PC->Xbox)
				pDesc->Format = EmuPC2XB_D3DFormat(SurfaceDesc.Format);
				pDesc->Type   = (X_D3DRESOURCETYPE)SurfaceDesc.Type;

				if(pDesc->Type > 7)
					CxbxKrnlCleanup("EmuIDirect3DSurface8_GetDesc: pDesc->Type > 7");

				pDesc->Usage  = SurfaceDesc.Usage;
				pDesc->Size   = SurfaceDesc.Size;

				// TODO: Convert from Xbox to PC!!
				if(SurfaceDesc.MultiSampleType == D3DMULTISAMPLE_NONE)
					pDesc->MultiSampleType = (D3DMULTISAMPLE_TYPE)0x0011;
				else
					CxbxKrnlCleanup("EmuIDirect3DSurface8_GetDesc Unknown Multisample format! (%d)", SurfaceDesc.MultiSampleType);

				pDesc->Width  = SurfaceDesc.Width;
				pDesc->Height = SurfaceDesc.Height;
			}
		}
		else
			hRet = D3DERR_INVALIDCALL;
	}
}
#endif

#if 0 // Patch disabled - calls Lock2DSurface
VOID WINAPI XTL::EMUPATCH(D3DSurface_LockRect)
(
    X_D3DResource      *pThis,
    D3DLOCKED_RECT     *pLockedRect,
    CONST RECT         *pRect,
    DWORD               Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(pLockedRect)
		LOG_FUNC_ARG(pRect)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;

    if(IsYuvSurface(pThis))
    {
        pLockedRect->Pitch = g_dwOverlayP;
        pLockedRect->pBits = (PVOID)pThis->Data;
    }
    else
    {
		CxbxUpdateResource(pThis);

		DWORD D3DLockFlags = 0;
		if (Flags & X_D3DLOCK_READONLY)
			D3DLockFlags |= D3DLOCK_READONLY;

		//if (Flags & X_D3DLOCK_TILED)
			//EmuWarning("D3DLOCK_TILED ignored!");

		if (!(Flags & X_D3DLOCK_READONLY) && !(Flags & X_D3DLOCK_TILED) && Flags != 0)
			CxbxKrnlCleanup("D3DSurface_LockRect: Unknown Flags! (0x%.08X)", Flags);

		// As it turns out, D3DSurface_LockRect can also be called with textures (not just surfaces)
		// so cater for that. TODO : Should we handle cube and volume textures here too?
		DWORD dwCommonType = GetXboxCommonResourceType(pThis);
		switch (dwCommonType) {
		case X_D3DCOMMON_TYPE_TEXTURE:
		{
			IDirect3DTexture8 *pHostTexture = GetHostTexture(pThis);
			if (pHostTexture == nullptr)
			{
				EmuWarning("Missing Texture!");
				hRet = E_FAIL;
			}
			else
			{
				pHostTexture->UnlockRect(0); // remove old lock
				hRet = pHostTexture->LockRect(0, pLockedRect, pRect, D3DLockFlags);
				DEBUG_D3DRESULT(hRet, "pHostTexture->LockRect");
			}
			break;
		}
		case X_D3DCOMMON_TYPE_SURFACE:
		{
			IDirect3DSurface8 *pHostSurface = GetHostSurface(pThis);
			if (pHostSurface == nullptr)
			{
				EmuWarning("Missing Surface!");
				hRet = E_FAIL;
			}
			else
			{
				pHostSurface->UnlockRect(); // remove old lock
				hRet = pHostSurface->LockRect(pLockedRect, pRect, D3DLockFlags);
				DEBUG_D3DRESULT(hRet, "pHostSurface->LockRect");
			}
			break;
		}
		default:
			EmuWarning("D3DSurface_LockRect: Unhandled type!");
		}
	}
}
#endif

#if 0 // Patch disabled
DWORD WINAPI XTL::EMUPATCH(D3DBaseTexture_GetLevelCount)
(
    X_D3DBaseTexture   *pThis
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pThis);

	DWORD dwRet = 0;

	if(pThis != NULL)
	{
		EmuVerifyResourceIsRegistered(pThis);

		IDirect3DBaseTexture8 *pHostBaseTexture = GetHostBaseTexture(pThis);

		if(pHostBaseTexture != nullptr)
			dwRet = pHostBaseTexture->GetLevelCount();
	}

    return dwRet;
}
#endif

#if 0 // Patch disabled
XTL::X_D3DSurface * WINAPI XTL::EMUPATCH(D3DTexture_GetSurfaceLevel2)
(
    X_D3DTexture   *pThis,
    UINT            Level
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_END;

    X_D3DSurface *result = NULL;

	if (pThis == NULL)
		EmuWarning("pThis not assigned!");
	else
	{
		EmuVerifyResourceIsRegistered(pThis);
		if (IsYuvSurface(pThis))
		{
			result = (X_D3DSurface*)pThis;
		}
		else
		{
			result = EmuNewD3DSurface();
			result->Data = CXBX_D3DRESOURCE_DATA_SURFACE_LEVEL;
			result->Format = 0; // TODO : Set this
			result->Size = 0; // TODO : Set this

			IDirect3DTexture8 *pHostTexture = GetHostTexture(pThis);
			IDirect3DSurface8 *pNewHostSurface = nullptr;

			HRESULT hRet = pHostTexture->GetSurfaceLevel(Level, &pNewHostSurface);
			DEBUG_D3DRESULT(hRet, "pHostTexture->GetSurfaceLevel");

			if (SUCCEEDED(hRet))
				SetHostSurface(result, pNewHostSurface);

			result->Parent = pThis;
			pThis->Common++; // AddRef Parent too
		}
		
		result->Common++; // Don't EMUPATCH(D3DResource_AddRef)(result) - that would AddRef Parent one too many
	}
	
    RETURN(result);
}
#endif

#if 0 // Patch disabled - just calls Lock2DSurface
VOID WINAPI XTL::EMUPATCH(D3DTexture_LockRect)
(
    X_D3DTexture   *pThis,
    UINT            Level,
    D3DLOCKED_RECT *pLockedRect,
    CONST RECT     *pRect,
    DWORD           Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pLockedRect)
		LOG_FUNC_ARG(pRect)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;

    EmuVerifyResourceIsRegistered(pThis);

    // check if we have an unregistered YUV2 resource
    if( (pThis != NULL) && IsYuvSurface(pThis))
    {
        pLockedRect->Pitch = g_dwOverlayP;
        pLockedRect->pBits = (PVOID)pThis->Data;

        hRet = D3D_OK;
    }
    else
    {
        IDirect3DTexture8 *pHostTexture = GetHostTexture(pThis);

        DWORD NewFlags = 0;

        if(Flags & X_D3DLOCK_READONLY)
            NewFlags |= D3DLOCK_READONLY;

        //if(Flags & X_D3DLOCK_TILED)
            //EmuWarning("D3DLOCK_TILED ignored!"); 

        if(Flags & X_D3DLOCK_NOOVERWRITE)
            NewFlags |= D3DLOCK_NOOVERWRITE;

        if(Flags & X_D3DLOCK_NOFLUSH)
            EmuWarning("D3DLOCK_NOFLUSH ignored!");

        if(!(Flags & X_D3DLOCK_READONLY) && !(Flags & X_D3DLOCK_TILED) && !(Flags & X_D3DLOCK_NOOVERWRITE) && !(Flags & X_D3DLOCK_NOFLUSH) && Flags != 0)
            CxbxKrnlCleanup("EmuIDirect3DTexture8_LockRect: Unknown Flags! (0x%.08X)", Flags);

		if (pHostTexture != nullptr)
		{
			pHostTexture->UnlockRect(Level); // remove old lock
			hRet = pHostTexture->LockRect(Level, pLockedRect, pRect, NewFlags);
		}

		pThis->Common |= X_D3DCOMMON_ISLOCKED;
	}
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DTexture_GetSurfaceLevel)
(
    X_D3DTexture       *pThis,
    UINT                Level,
    X_D3DSurface      **ppSurfaceLevel
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DTexture_GetSurfaceLevel2");

	*ppSurfaceLevel = EMUPATCH(D3DTexture_GetSurfaceLevel2)(pThis, Level);

    return D3D_OK;
}
#endif

#if 0 // Patch disabled - Just calls Lock3DSurface
VOID WINAPI XTL::EMUPATCH(D3DVolumeTexture_LockBox)
(
    X_D3DVolumeTexture *pThis,
    UINT                Level,
    D3DLOCKED_BOX      *pLockedVolume,
    CONST D3DBOX       *pBox,
    DWORD               Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pLockedVolume)
		LOG_FUNC_ARG(pBox)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    EmuVerifyResourceIsRegistered(pThis);

    IDirect3DVolumeTexture8 *pHostVolumeTexture = GetHostVolumeTexture(pThis);

	pHostVolumeTexture->UnlockBox(Level); // remove old lock
    HRESULT hRet = pHostVolumeTexture->LockBox(Level, pLockedVolume, pBox, Flags);
	DEBUG_D3DRESULT(hRet, "pHostVolumeTexture->LockBox");
}
#endif

#if 0 // Patch disabled - just calls Lock2DSurface
VOID WINAPI XTL::EMUPATCH(D3DCubeTexture_LockRect)
(
    X_D3DCubeTexture   *pThis,
    D3DCUBEMAP_FACES    FaceType,
    UINT                Level,
    D3DLOCKED_RECT     *pLockedBox,
    CONST RECT         *pRect,
    DWORD               Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(FaceType)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pLockedBox)
		LOG_FUNC_ARG(pRect)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    EmuVerifyResourceIsRegistered(pThis);

    IDirect3DCubeTexture8 *pHostCubeTexture = GetHostCubeTexture(pThis);

	pHostCubeTexture->UnlockRect(FaceType, Level); // remove old lock
	HRESULT hRet = pHostCubeTexture->LockRect(FaceType, Level, pLockedBox, pRect, Flags);    
}
#endif

#if 0 // Patch disabled
ULONG WINAPI XTL::EMUPATCH(D3DDevice_Release)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	// See GetD3DResourceRefCount()
    g_pD3DDevice8->AddRef();
    DWORD RefCount = g_pD3DDevice8->Release();
    if (RefCount == 1)
    {
		while (g_EmuCDPD.bSignalled)
			Sleep(10);

		// Signal proxy thread, and wait for completion
        g_EmuCDPD.bCreate = false;
        g_EmuCDPD.bSignalled = true;

        while (g_EmuCDPD.bSignalled)
            Sleep(10);

        RefCount = g_EmuCDPD.hRet;
    }
    else
    {
        RefCount = g_pD3DDevice8->Release();
    }

    return RefCount;
}
#endif

#if 0 // Patch disabled
3HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateVertexBuffer)
(
    UINT                Length,
    DWORD               Usage,
    DWORD               FVF,
    D3DPOOL             Pool,
    X_D3DVertexBuffer **ppVertexBuffer
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_CreateVertexBuffer2");

	*ppVertexBuffer = EMUPATCH(D3DDevice_CreateVertexBuffer2)(Length);

    return D3D_OK;
}
#endif

#if 0 // Patch disabled
XTL::X_D3DVertexBuffer* WINAPI XTL::EMUPATCH(D3DDevice_CreateVertexBuffer2)
(
    UINT Length
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Length);

    X_D3DVertexBuffer *pD3DVertexBuffer = EmuNewD3DVertexBuffer();
	IDirect3DVertexBuffer8  *pNewHostVertexBuffer = nullptr;
	
    HRESULT hRet = g_pD3DDevice8->CreateVertexBuffer
    (
        Length,
        0,
        0,
        D3DPOOL_MANAGED,
        &pNewHostVertexBuffer
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexBuffer");

    if(SUCCEEDED(hRet))
		SetHostVertexBuffer(pD3DVertexBuffer, pNewHostVertexBuffer);

    #ifdef _DEBUG_TRACK_VB
    g_VBTrackTotal.insert(pNewHostVertexBuffer);
    #endif

    RETURN(pD3DVertexBuffer);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_EnableOverlay)
(
    BOOL Enable
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Enable);

    if(Enable == FALSE && (g_pDDSOverlay7 != nullptr))
    {
        g_pDDSOverlay7->UpdateOverlay(nullptr, g_pDDSPrimary7, nullptr, DDOVER_HIDE, nullptr);

        // cleanup overlay clipper
        if(g_pDDClipper != nullptr)
        {
            g_pDDClipper->Release();
            g_pDDClipper = nullptr;
        }

        // cleanup overlay surface
        if(g_pDDSOverlay7 != nullptr)
        {
            g_pDDSOverlay7->Release();
            g_pDDSOverlay7 = nullptr;
        }
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_UpdateOverlay)
(
	X_D3DSurface *pSurface,
    CONST RECT   *SrcRect,
    CONST RECT   *DstRect,
    BOOL          EnableColorKey,
    D3DCOLOR      ColorKey
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pSurface)
		LOG_FUNC_ARG(SrcRect)
		LOG_FUNC_ARG(DstRect)
		LOG_FUNC_ARG(EnableColorKey)
		LOG_FUNC_ARG(ColorKey)
		LOG_FUNC_END;

	if (pSurface == NULL)
		CxbxKrnlCleanup("D3DDevice_UpdateOverlay shouldn't recieve NULL"); // At least, not anymore now we've stopped hacking this

	// Get the overlay measurements :
	UINT g_dwOverlayW, g_dwOverlayH, g_dwOverlayP, dwSize;
	CxbxGetPixelContainerMeasures(pSurface, 0, &g_dwOverlayW, &g_dwOverlayH, &g_dwOverlayP, &dwSize);

	// delayed from D3DDevice_EnableOverlay :
	if (g_pDDSOverlay7 == nullptr)
	{
		// initialize overlay surface
		// manually copy data over to overlay
		if (g_bSupportsYUY2Overlay)
		{
			DDSURFACEDESC2 ddsd2 = { 0 };

			ddsd2.dwSize = sizeof(ddsd2);
			ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
			ddsd2.ddsCaps.dwCaps = DDSCAPS_OVERLAY;
			ddsd2.dwWidth = g_dwOverlayW;
			ddsd2.dwHeight = g_dwOverlayH;
			ddsd2.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
			ddsd2.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
			ddsd2.ddpfPixelFormat.dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');

			HRESULT hRet = g_pDD7->CreateSurface(&ddsd2, &g_pDDSOverlay7, nullptr);
			DEBUG_D3DRESULT(hRet, "g_pDD7->CreateSurface");

			if (FAILED(hRet))
				CxbxKrnlCleanup("Could not create overlay surface");

			hRet = g_pDD7->CreateClipper(0, &g_pDDClipper, nullptr);
			DEBUG_D3DRESULT(hRet, "g_pDD7->CreateClipper");

			if (FAILED(hRet))
				CxbxKrnlCleanup("Could not create overlay clipper");

			hRet = g_pDDClipper->SetHWnd(0, g_hEmuWindow);
		}
	}

	uint08 *pYUY2SourceBuffer = (uint08*)GetDataFromXboxResource(pSurface);
	RECT EmuSourRect;
	RECT EmuDestRect;

	if (SrcRect != NULL) {
		EmuSourRect = *SrcRect;
	} else {
		SetRect(&EmuSourRect, 0, 0, g_dwOverlayW, g_dwOverlayH);
	}

	if (DstRect != NULL) {
		// If there's a destination rectangle given, copy that into our local variable :
		EmuDestRect = *DstRect;
	} else {
		GetClientRect(g_hEmuWindow, &EmuDestRect);
	}

	// manually copy data over to overlay
	if(g_bSupportsYUY2Overlay)
	{
		// Make sure the overlay is allocated before using it
		if (g_pDDSOverlay7 == nullptr) {
			EMUPATCH(D3DDevice_EnableOverlay)(TRUE);

			assert(g_pDDSOverlay7 != nullptr);
		}

		DDSURFACEDESC2  ddsd2 = { 0 };
		ddsd2.dwSize = sizeof(ddsd2);

		HRESULT hRet;
		hRet = g_pDDSOverlay7->Lock(nullptr, &ddsd2, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
		DEBUG_D3DRESULT(hRet, "g_pDDSOverlay7->Lock - Unable to lock overlay surface!");

		if(SUCCEEDED(hRet))
		{
			// copy data
			DWORD dwOverlayWidthInBytes = g_dwOverlayW * 2; // 2 = EmuXBFormatBytesPerPixel(D3DFMT_YUY2);

			CxbxPitchedCopy(
				(BYTE *)ddsd2.lpSurface, pYUY2SourceBuffer,
				ddsd2.lPitch, g_dwOverlayP,
				dwOverlayWidthInBytes, g_dwOverlayH);

			hRet = g_pDDSOverlay7->Unlock(nullptr);
			DEBUG_D3DRESULT(hRet, "g_pDDSOverlay7->Unlock");
		}

		// update overlay!
		DWORD dwUpdateFlags = DDOVER_SHOW;
		DDOVERLAYFX ddofx = { 0 };

		ddofx.dwSize = sizeof(DDOVERLAYFX);
		if (EnableColorKey) {
			if (g_DriverCaps.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY)
			{
				dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE | DDOVER_DDFX;
				ddofx.dckDestColorkey.dwColorSpaceLowValue = ColorKey;
				ddofx.dckDestColorkey.dwColorSpaceHighValue = ColorKey;
			}
		}  else {
			if (g_DriverCaps.dwCKeyCaps & DDCKEYCAPS_SRCOVERLAY)
			{
				dwUpdateFlags |= DDOVER_KEYSRCOVERRIDE | DDOVER_DDFX;
				ddofx.dckSrcColorkey.dwColorSpaceLowValue = ColorKey;
				ddofx.dckSrcColorkey.dwColorSpaceHighValue = ColorKey;
			}
		}

		// Get the screen (multi-monitor-compatible) coordinates of the client area :
		MapWindowPoints(g_hEmuWindow, HWND_DESKTOP, (LPPOINT)&EmuDestRect, 2);

		hRet = g_pDDSOverlay7->UpdateOverlay(&EmuSourRect, g_pDDSPrimary7, &EmuDestRect, dwUpdateFlags, &ddofx);
		DEBUG_D3DRESULT(hRet, "g_pDDSOverlay7->UpdateOverlay");
	}
	else // No hardware overlay
	{
		HRESULT hRet;

		// load the YUY2 into the backbuffer
		CxbxGetActiveHostBackBuffer();

		// Get backbuffer dimenions; TODO : remember this once, at creation/resize time
		D3DSURFACE_DESC BackBufferDesc;
		hRet = g_pActiveHostBackBuffer->GetDesc(&BackBufferDesc);
		DEBUG_D3DRESULT(hRet, "g_pActiveHostBackBuffer->GetDesc");

		// Limit the width and height of the output to the backbuffer dimensions.
		// This will (hopefully) prevent exceptions in Blinx - The Time Sweeper 
		// (see https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/issues/285)
		{
			// Use our (bounded) copy when bounds exceed :
			if (EmuDestRect.right > (LONG)BackBufferDesc.Width) {
				EmuDestRect.right = (LONG)BackBufferDesc.Width;
				DstRect = &EmuDestRect;
			}

			if (EmuDestRect.bottom > (LONG)BackBufferDesc.Height) {
				EmuDestRect.bottom = (LONG)BackBufferDesc.Height;
				DstRect = &EmuDestRect;
			}
		}

		// Use D3DXLoadSurfaceFromMemory() to do conversion, stretching and filtering
		// avoiding the need for YUY2toARGB() (might become relevant when porting to D3D9 or OpenGL)
		// see https://msdn.microsoft.com/en-us/library/windows/desktop/bb172902(v=vs.85).aspx
		hRet = D3DXLoadSurfaceFromMemory(
			/* pDestSurface = */ g_pActiveHostBackBuffer,
			/* pDestPalette = */ nullptr, // Palette not needed for YUY2
			/* pDestRect = */DstRect, // Either the unmodified original (can be NULL) or a pointer to our local variable
			/* pSrcMemory = */ pYUY2SourceBuffer, // Source buffer
			/* SrcFormat = */ D3DFMT_YUY2,
			/* SrcPitch = */ g_dwOverlayP,
			/* pSrcPalette = */ nullptr, // Palette not needed for YUY2
			/* SrcRect = */ &EmuSourRect,
			/* Filter = */ D3DX_FILTER_POINT, // Dxbx note : D3DX_FILTER_LINEAR gives a smoother image, but 'bleeds' across borders
			/* ColorKey = */ EnableColorKey ? ColorKey : 0);
		DEBUG_D3DRESULT(hRet, "D3DXLoadSurfaceFromMemory - UpdateOverlay could not convert buffer!\n");

		// Update overlay if present was not called since the last call to
		// EmuD3DDevice_UpdateOverlay.
		if(g_bHackUpdateSoftwareOverlay)
			CxbxPresent();

		g_bHackUpdateSoftwareOverlay = TRUE;
	}
}

BOOL WINAPI XTL::EMUPATCH(D3DDevice_GetOverlayUpdateStatus)()
{
	FUNC_EXPORTS

	LOG_FUNC();    

	LOG_UNIMPLEMENTED();

    // TODO: Actually check for update status
    return FALSE;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_BlockUntilVerticalBlank)()
{
	FUNC_EXPORTS

	LOG_FUNC();

	std::unique_lock<std::mutex> lk(g_VBConditionMutex);
	g_VBConditionVariable.wait(lk);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVerticalBlankCallback)
(
    X_D3DVBLANKCALLBACK pCallback
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pCallback);

    g_pVBCallback = pCallback;    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTextureState_TexCoordIndex)
(
    DWORD Stage,
    DWORD Value
)
{
	FUNC_EXPORTS

	// TODO: Xbox Direct3D supports sphere mapping OpenGL style.

	CxbxInternalSetTextureStageState(__func__, Stage, X_D3DTSS_TEXCOORDINDEX, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTextureState_BorderColor)
(
    DWORD Stage,
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetTextureStageState(__func__, Stage, X_D3DTSS_BORDERCOLOR, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTextureState_ColorKeyColor)
(
    DWORD Stage,
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetTextureStageState(__func__, Stage, X_D3DTSS_COLORKEYCOLOR, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTextureState_BumpEnv)
(
    DWORD                      Stage,
    X_D3DTEXTURESTAGESTATETYPE Type,
    DWORD                      Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetTextureStageState(__func__, Stage, XTL::DxbxFromOldVersion_D3DTSS(Type), Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTextureStageStateNotInline)
(
	DWORD                      Stage,
	X_D3DTEXTURESTAGESTATETYPE Type,
	DWORD                      Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetTextureStageState(__func__, Stage, XTL::DxbxFromOldVersion_D3DTSS(Type), Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_TwoSidedLighting)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_TWOSIDEDLIGHTING, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_BackFillMode)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	// blueshogun96 12/4/07
	// I haven't had access to Cxbx sources in a few months, great to be back :)
	//
	// Anyway, since standard Direct3D doesn't support the back fill mode
	// operation, this function will be ignored.  Things like this make me
	// think even more that an OpenGL port wouldn't hurt since OpenGL supports
	// nearly all of the missing features that Direct3D lacks.  The Xbox's version
	// of Direct3D was specifically created to take advantage of certain NVIDIA
	// GPU registers and provide more OpenGL-like features IHMO.

	CxbxInternalSetRenderState(__func__, X_D3DRS_BACKFILLMODE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_FrontFace)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_FRONTFACE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_LogicOp)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_LOGICOP, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_NormalizeNormals)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_NORMALIZENORMALS, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_TextureFactor)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_TEXTUREFACTOR, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_ZBias)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_ZBIAS, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_EdgeAntiAlias)
(
    DWORD Value
)
{
	FUNC_EXPORTS

//  TODO: Analyze performance and compatibility (undefined behavior on PC with triangles or points)
	CxbxInternalSetRenderState(__func__, X_D3DRS_EDGEANTIALIAS, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_FillMode)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_FILLMODE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_FogColor)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_FOGCOLOR, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_Dxt1NoiseEnable)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_DXT1NOISEENABLE, Value);
}

VOID __fastcall XTL::EMUPATCH(D3DDevice_SetRenderState_Simple)
(
    DWORD Method,
    DWORD Value
)
{
	FUNC_EXPORTS

	X_D3DRENDERSTATETYPE XboxRenderState = DxbxXboxMethodToRenderState(Method);

	if (XboxRenderState == X_D3DRS_UNKNOWN) {
		LOG_FUNC_BEGIN
			LOG_FUNC_ARG(Method)
			LOG_FUNC_ARG(Value)
			LOG_FUNC_END;

		EmuWarning("D3DDevice_SetRenderState_Simple(0x%.08X, 0x%.08X) : Unknown NV2A method!", Method, Value);
		return;
	}

	// Use a helper for the simple render states, as SetRenderStateNotInline
	// needs to be able to call it too :
	CxbxInternalSetRenderState(__func__, XboxRenderState, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_VertexBlend)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_VERTEXBLEND, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_PSTextureModes)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_PSTEXTUREMODES, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_CullMode)
(
    DWORD Value // X_D3DCULL
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_CULLMODE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_LineWidth)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_LINEWIDTH, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_StencilFail)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_STENCILFAIL, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_OcclusionCullEnable)
(
    DWORD Value // BOOL
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_OCCLUSIONCULLENABLE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_StencilCullEnable)
(
    DWORD Value // BOOL
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_STENCILCULLENABLE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_RopZCmpAlwaysRead)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_ROPZCMPALWAYSREAD, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_RopZRead)
(
    DWORD Value
)
{
	FUNC_EXPORTS
		
	CxbxInternalSetRenderState(__func__, X_D3DRS_ROPZREAD, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_DoNotCullUncompressed)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_DONOTCULLUNCOMPRESSED, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_ZEnable)
(
    DWORD Value // BOOL
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_ZENABLE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_StencilEnable)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_STENCILENABLE, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_MultiSampleAntiAlias)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_MULTISAMPLEANTIALIAS, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_MultiSampleMask)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_MULTISAMPLEMASK, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_MultiSampleMode)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_MULTISAMPLEMODE, Value);
#if 0 // TODO : Dxbx has this too - is it necessary?
	pRenderTarget = g_EmuD3DActiveRenderTarget;
	if (pRenderTarget == g_EmuD3DFrameBuffers[0])
		EMUPATCH(D3DDevice_SetRenderTarget)(pRenderTarget, g_EmuD3DActiveDepthStencil);
#endif
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_MultiSampleRenderTargetMode)
(
    DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_MULTISAMPLERENDERTARGETMODE, Value);
#if 0 // TODO : Dxbx has this too - is it necessary?
	pRenderTarget = g_EmuD3DActiveRenderTarget;
	if (pRenderTarget == g_EmuD3DFrameBuffers[0])
		EMUPATCH(D3DDevice_SetRenderTarget)(pRenderTarget, g_EmuD3DActiveDepthStencil);
#endif
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_ShadowFunc)
(
    DWORD Value
)
{
	FUNC_EXPORTS

    // ShadowFunc reflects the following Xbox-only extension
    //
    // typedef enum _D3DRENDERSTATETYPE {
    //   ...
    //   D3DRS_SHADOWFUNC = 156, // D3DCMPFUNC
    //   ...
    // } D3DRENDERSTATETYPE;
    //
    // Value is a member of the D3DCMPFUNC enumeration that 
    // specifies what function to use with a shadow buffer. 
    // The default value is D3DCMP_NEVER. 

	CxbxInternalSetRenderState(__func__, X_D3DRS_SHADOWFUNC, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_YuvEnable)
(
    BOOL Enable
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_YUVENABLE, (DWORD)Enable);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderState_SampleAlpha)
(
	DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, X_D3DRS_SAMPLEALPHA, Value);
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderStateNotInline)
(
	DWORD State,
	DWORD Value
)
{
	FUNC_EXPORTS

	CxbxInternalSetRenderState(__func__, XTL::DxbxVersionAdjust_D3DRS(State), Value);
}

#if 0 // Patch disabled - Dxbx note : Disabled, as we DO have Xbox_D3D__RenderState_Deferred pin-pointed correctly
VOID __fastcall XTL::EMUPATCH(D3DDevice_SetRenderState_Deferred)
(
	DWORD State,
	DWORD Value
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(State)
		LOG_FUNC_ARG(Value)
		LOG_FUNC_END;

	// TODO: HACK: Technically, this function doesn't need to be emulated.
	// The location of Xbox_D3D__RenderState_Deferred for 3911 isn't correct and at
	// the time of writing, I don't understand how to fix it.  Until then, 
	// I'm going to implement this in a reckless manner.  When the offset for
	// Xbox_D3D__RenderState_Deferred is fixed for 3911, this function should be
	// obsolete!

	if (State > 81 && State < 116)
		Xbox_D3D__RenderState_Deferred[State - 82] = Value;
	else
		CxbxKrnlCleanup("Unknown Deferred RenderState! (%d)\n", State);

}
#endif

// TODO : Disable all transform patches (D3DDevice__[Get|Set][Transform|ModelView]), once we read Xbox matrices on time
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetTransform)
(
	X_D3DTRANSFORMSTATETYPE State,
    CONST D3DMATRIX      *pMatrix
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(State)
		LOG_FUNC_ARG(pMatrix)
		LOG_FUNC_END;

    /*
    printf("pMatrix (%d)\n", State);
    printf("{\n");
    printf("    %.08f,%.08f,%.08f,%.08f\n", pMatrix->_11, pMatrix->_12, pMatrix->_13, pMatrix->_14);
    printf("    %.08f,%.08f,%.08f,%.08f\n", pMatrix->_21, pMatrix->_22, pMatrix->_23, pMatrix->_24);
    printf("    %.08f,%.08f,%.08f,%.08f\n", pMatrix->_31, pMatrix->_32, pMatrix->_33, pMatrix->_34);
    printf("    %.08f,%.08f,%.08f,%.08f\n", pMatrix->_41, pMatrix->_42, pMatrix->_43, pMatrix->_44);
    printf("}\n");

    if(State == 6 && (pMatrix->_11 == 1.0f) && (pMatrix->_22 == 1.0f) && (pMatrix->_33 == 1.0f) && (pMatrix->_44 == 1.0f))
    {
        g_bSkipPush = TRUE;
        printf("SkipPush ON\n");
    }
    else
    {
        g_bSkipPush = FALSE;
        printf("SkipPush OFF\n");
    }
    */

	D3DTRANSFORMSTATETYPE PCState = EmuXB2PC_D3DTS(State);

    HRESULT hRet = g_pD3DDevice8->SetTransform(PCState, pMatrix);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetTransform");    
}

// TODO : Disable all transform patches (D3DDevice__[Get|Set][Transform|ModelView]), once we read Xbox matrices on time
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetTransform)
(
	X_D3DTRANSFORMSTATETYPE State,
    D3DMATRIX            *pMatrix
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(State)
		LOG_FUNC_ARG(pMatrix)
		LOG_FUNC_END;

	D3DTRANSFORMSTATETYPE PCState = EmuXB2PC_D3DTS(State);

    HRESULT hRet = g_pD3DDevice8->GetTransform(PCState, pMatrix);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetTransform");    
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DVertexBuffer_Lock)
(
    X_D3DVertexBuffer  *pVertexBuffer,
    UINT                OffsetToLock,
    UINT                SizeToLock,
    BYTE              **ppbData,
    DWORD               Flags
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DVertexBuffer_Lock2");

	*ppbData = EMUPATCH(D3DVertexBuffer_Lock2)(pVertexBuffer, Flags) + OffsetToLock;
	DbgPrintf("D3DVertexBuffer_Lock returns 0x%p\n", *ppbData);
}
#endif

#if 0 // Patch disabled - Xbox reads g_pDevice, calls StartPush, EndPush, BlockOnNonSurfaceResource
BYTE* WINAPI XTL::EMUPATCH(D3DVertexBuffer_Lock2)
(
    X_D3DVertexBuffer  *pVertexBuffer,
    DWORD               Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pVertexBuffer)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

	// Test case : X-Marbles
    BYTE *pbNativeData = (BYTE *)GetDataFromXboxResource(pVertexBuffer);

	DbgPrintf("D3DVertexBuffer_Lock2 returns 0x%p\n", pbNativeData);
	return pbNativeData; // TODO : Fix BYTE* logging, so we can use RETURN(pbNativeData);
}
#endif

#if 0 // Patch disabled - Reads Xbox g_Stream[StreamNumber].pVertexBuffer
XTL::X_D3DVertexBuffer* WINAPI XTL::EMUPATCH(D3DDevice_GetStreamSource)
(
    UINT  StreamNumber,
    UINT *pStride
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(StreamNumber)
		LOG_FUNC_ARG(pStride)
		LOG_FUNC_END;

	X_D3DVertexBuffer* pVertexBuffer = NULL;
	*pStride = 0;

	if (StreamNumber < MAX_NBR_STREAMS)
	{ 
		pVertexBuffer = g_D3DStreams[StreamNumber];
		if (pVertexBuffer != NULL)
		{
			pVertexBuffer->Common++; // EMUPATCH(D3DResource_AddRef)(pVertexBuffer) would give too much overhead (and needless logging)
			*pStride = g_D3DStreamStrides[StreamNumber];
		}
	}

    RETURN(pVertexBuffer);
}
#endif

#if 0 // Patch disabled
// TODO : Introduce OOVPA for D3DDevice_GetStreamSource2
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetStreamSource2)
(
	UINT                StreamNumber,
	X_D3DVertexBuffer **ppStreamData,
	UINT               *pStride
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_GetStreamSource");

	*ppStreamData = EMUPATCH(D3DDevice_GetStreamSource)(StreamNumber, pStride);
	return D3D_OK;
}
#endif

#if 0 // Patch disabled - Writes Xbox g_Stream[StreamNumber].pVertexBuffer
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetStreamSource)
(
    UINT                StreamNumber,
    X_D3DVertexBuffer  *pStreamData,
    UINT                Stride
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(StreamNumber)
		LOG_FUNC_ARG(pStreamData)
		LOG_FUNC_ARG(Stride)
		LOG_FUNC_END;

	if (StreamNumber < MAX_NBR_STREAMS)
	{
		/* TODO : Call into unpatched D3DResource_Release function :
		if (g_D3DStreams[StreamNumber] != NULL)
			EMUPATCH(D3DResource_Release)(g_D3DStreams[StreamNumber]);*/

		// Remember these for D3DDevice_GetStreamSource to read:
		g_D3DStreams[StreamNumber] = pStreamData;
		g_D3DStreamStrides[StreamNumber] = Stride;
	}

    IDirect3DVertexBuffer8 *pHostVertexBuffer = nullptr;

    if(pStreamData != NULL)
		pHostVertexBuffer = CxbxUpdateVertexBuffer(pStreamData);

#ifdef _DEBUG_TRACK_VB
	if (pHostVertexBuffer != nullptr)
		g_bVBSkipStream = g_VBTrackDisable.exists(pHostVertexBuffer);
#endif

    HRESULT hRet = g_pD3DDevice8->SetStreamSource(StreamNumber, pHostVertexBuffer, Stride);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetStreamSource");

	if(FAILED(hRet))
        CxbxKrnlCleanup("SetStreamSource Failed!");
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexShader)
(
    DWORD Handle
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Handle);

    HRESULT hRet = D3D_OK;

    g_CurrentVertexShader = Handle;

    // Store viewport offset and scale in constant registers 58 (c-38) and
    // 59 (c-37) used for screen space transformation.
    if (g_VertexShaderConstantMode != X_D3DSCM_NORESERVEDCONSTANTS) {
        // TODO: Proper solution.
        static float vScale[] = { (2.0f / 640), (-2.0f / 480), 0.0f, 0.0f };
        static float vOffset[] = { -1.0f, 1.0f, 0.0f, 1.0f };

        g_pD3DDevice8->SetVertexShaderConstant(58, vScale, 1);
        g_pD3DDevice8->SetVertexShaderConstant(59, vOffset, 1);
    }

    DWORD HostVertexHandle;

	CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
	if (pHostVertexShader != nullptr) {
		HostVertexHandle = pHostVertexShader->Handle;
    }
    else {
        HostVertexHandle = Handle;
		HostVertexHandle &= ~D3DFVF_XYZ;
		HostVertexHandle &= ~D3DFVF_XYZRHW;
		HostVertexHandle &= ~D3DFVF_XYZB1;
		HostVertexHandle &= ~D3DFVF_XYZB2;
		HostVertexHandle &= ~D3DFVF_XYZB3;
		HostVertexHandle &= ~D3DFVF_XYZB4;
		HostVertexHandle &= ~D3DFVF_DIFFUSE;
		HostVertexHandle &= ~D3DFVF_NORMAL;
		HostVertexHandle &= ~D3DFVF_SPECULAR;
		HostVertexHandle &= 0x00FF;
		if (HostVertexHandle != 0)
			EmuWarning("EmuD3DDevice_SetVertexShader Handle = 0x%.08X", HostVertexHandle);

		HostVertexHandle = Handle;
    }

	hRet = g_pD3DDevice8->SetVertexShader(HostVertexHandle);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader");    
}

// TODO : Move to own file
constexpr UINT QuadToTriangleVertexCount(UINT NrOfQuadVertices)
{
	return (NrOfQuadVertices * VERTICES_PER_TRIANGLE * TRIANGLES_PER_QUAD) / VERTICES_PER_QUAD;
}

// TODO : Move to own file
bool WindingClockwise = true;
constexpr uint IndicesPerPage = PAGE_SIZE / sizeof(XTL::INDEX16);
constexpr uint InputQuadsPerPage = ((IndicesPerPage * VERTICES_PER_QUAD) / VERTICES_PER_TRIANGLE) / TRIANGLES_PER_QUAD;

// TODO : Move to own file
XTL::INDEX16 *CxbxAssureQuadListIndexBuffer(UINT NrOfQuadVertices)
{
	if (QuadToTriangleIndexBuffer_Size < NrOfQuadVertices)
	{
		QuadToTriangleIndexBuffer_Size = RoundUp(NrOfQuadVertices, InputQuadsPerPage);

		UINT NrOfTriangleVertices = QuadToTriangleVertexCount(QuadToTriangleIndexBuffer_Size);

		if (pQuadToTriangleIndexBuffer != nullptr)
			free(pQuadToTriangleIndexBuffer);

		pQuadToTriangleIndexBuffer = (XTL::INDEX16 *)malloc(sizeof(XTL::INDEX16) * NrOfTriangleVertices);

		UINT i = 0;
		XTL::INDEX16 j = 0;
		while (i + 2 < NrOfTriangleVertices)
		{
			if (WindingClockwise) {
				// ABCD becomes ABC+CDA, so this is triangle 1 :
				pQuadToTriangleIndexBuffer[i + 0] = j + 0;
				pQuadToTriangleIndexBuffer[i + 1] = j + 1;
				pQuadToTriangleIndexBuffer[i + 2] = j + 2;
				i += VERTICES_PER_TRIANGLE;

				// And this is triangle 2 :
				pQuadToTriangleIndexBuffer[i + 0] = j + 2;
				pQuadToTriangleIndexBuffer[i + 1] = j + 3;
				pQuadToTriangleIndexBuffer[i + 2] = j + 0;
				i += VERTICES_PER_TRIANGLE;
			}
			else
			{
				// ABCD becomes ADC+CBA, so this is triangle 1 :
				pQuadToTriangleIndexBuffer[i + 0] = j + 0;
				pQuadToTriangleIndexBuffer[i + 1] = j + 3;
				pQuadToTriangleIndexBuffer[i + 2] = j + 2;
				i += VERTICES_PER_TRIANGLE;

				// And this is triangle 2 :
				pQuadToTriangleIndexBuffer[i + 0] = j + 2;
				pQuadToTriangleIndexBuffer[i + 1] = j + 1;
				pQuadToTriangleIndexBuffer[i + 2] = j + 0;
				i += VERTICES_PER_TRIANGLE;
			}

			// Next quad, please :
			j += VERTICES_PER_QUAD;
		}
	}

	return pQuadToTriangleIndexBuffer;
}

// TODO : Move to own file
void CxbxAssureQuadListD3DIndexBuffer(UINT NrOfQuadVertices)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	HRESULT hRet;

	if (QuadToTriangleD3DIndexBuffer_Size < NrOfQuadVertices)
	{
		// Round the number of indices up so we'll allocate whole pages
		QuadToTriangleD3DIndexBuffer_Size = RoundUp(NrOfQuadVertices, InputQuadsPerPage);
		UINT NrOfTriangleVertices = QuadToTriangleVertexCount(QuadToTriangleD3DIndexBuffer_Size); // 4 > 6
		UINT uiIndexBufferSize = sizeof(XTL::INDEX16) * NrOfTriangleVertices;

		// Create a new native index buffer of the above determined size :
		if (pQuadToTriangleD3DIndexBuffer != nullptr) {
			pQuadToTriangleD3DIndexBuffer->Release();
			pQuadToTriangleD3DIndexBuffer = nullptr;
		}

		hRet = g_pD3DDevice8->CreateIndexBuffer(
			uiIndexBufferSize,
			D3DUSAGE_WRITEONLY,
			XTL::D3DFMT_INDEX16,
			XTL::D3DPOOL_MANAGED,
			&pQuadToTriangleD3DIndexBuffer);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateIndexBuffer");

		if (FAILED(hRet))
			CxbxKrnlCleanup("CxbxAssureQuadListD3DIndexBuffer : IndexBuffer Create Failed!");

		// Put quadlist-to-triangle-list index mappings into this buffer :
		XTL::INDEX16* pIndexBufferData = nullptr;
		hRet = pQuadToTriangleD3DIndexBuffer->Lock(0, uiIndexBufferSize, (BYTE **)&pIndexBufferData, D3DLOCK_DISCARD);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateIndexBuffer");

		if (pIndexBufferData == nullptr)
			CxbxKrnlCleanup("CxbxAssureQuadListD3DIndexBuffer : Could not lock index buffer!");

		memcpy(pIndexBufferData, CxbxAssureQuadListIndexBuffer(NrOfQuadVertices), uiIndexBufferSize);

		pQuadToTriangleD3DIndexBuffer->Unlock();
	}

	// Activate the new native index buffer :
	hRet = g_pD3DDevice8->SetIndices(pQuadToTriangleD3DIndexBuffer, 0);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateIndexBuffer");

	if (FAILED(hRet))
		CxbxKrnlCleanup("CxbxAssureQuadListD3DIndexBuffer : SetIndices Failed!"); // +DxbxD3DErrorString(hRet));
}

// TODO : Move to own file
// Calls SetIndices with a separate index-buffer, that's populated with the supplied indices.
void CxbxDrawIndexedClosingLine(XTL::INDEX16 LowIndex, XTL::INDEX16 HighIndex)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	HRESULT hRet;

	const UINT uiIndexBufferSize = sizeof(XTL::INDEX16) * 2; // 4 bytes needed for 2 indices
	if (pClosingLineLoopIndexBuffer == nullptr) {
		hRet = g_pD3DDevice8->CreateIndexBuffer(uiIndexBufferSize, D3DUSAGE_WRITEONLY, XTL::D3DFMT_INDEX16, XTL::D3DPOOL_DEFAULT, &pClosingLineLoopIndexBuffer);
		if (FAILED(hRet))
			CxbxKrnlCleanup("Unable to create pClosingLineLoopIndexBuffer for D3DPT_LINELOOP emulation");
	}

	XTL::INDEX16 *pCxbxClosingLineLoopIndexBufferData = nullptr;
	hRet = pClosingLineLoopIndexBuffer->Lock(0, uiIndexBufferSize, (BYTE **)(&pCxbxClosingLineLoopIndexBufferData), D3DLOCK_DISCARD);
	DEBUG_D3DRESULT(hRet, "pClosingLineLoopIndexBuffer->Lock");

	pCxbxClosingLineLoopIndexBufferData[0] = LowIndex;
	pCxbxClosingLineLoopIndexBufferData[1] = HighIndex;

	hRet = pClosingLineLoopIndexBuffer->Unlock();
	DEBUG_D3DRESULT(hRet, "pClosingLineLoopIndexBuffer->Unlock");

	hRet = g_pD3DDevice8->SetIndices(pClosingLineLoopIndexBuffer, 0);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetIndices");

	hRet = g_pD3DDevice8->DrawIndexedPrimitive
	(
		XTL::D3DPT_LINELIST,
		LowIndex, // minIndex
		HighIndex - LowIndex + 1, // NumVertexIndices
		0, // startIndex
		1 // primCount
	);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitive(CxbxDrawIndexedClosingLine)");

	g_dwPrimPerFrame++;
}

// TODO : Move to own file
void CxbxDrawIndexedClosingLineUP(XTL::INDEX16 LowIndex, XTL::INDEX16 HighIndex, void *pHostVertexStreamZeroData, UINT uiHostVertexStreamZeroStride)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	XTL::INDEX16 CxbxClosingLineIndices[2] = { LowIndex, HighIndex };

	HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitiveUP(
		XTL::D3DPT_LINELIST,
		LowIndex, // MinVertexIndex
		HighIndex - LowIndex + 1, // NumVertexIndices,
		1, // PrimitiveCount,
		CxbxClosingLineIndices, // pIndexData
		XTL::D3DFMT_INDEX16, // IndexDataFormat
		pHostVertexStreamZeroData,
		uiHostVertexStreamZeroStride
	);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitiveUP(CxbxDrawIndexedClosingLineUP)");

	g_dwPrimPerFrame++;
}

// TODO : Move to own file
// Requires assigned pIndexData
// Called by D3DDevice_DrawIndexedVertices and EmuExecutePushBufferRaw (twice)
void XTL::CxbxDrawIndexed(CxbxDrawContext &DrawContext, INDEX16 *pIndexData)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	assert(DrawContext.dwStartVertex == 0);
	assert(pIndexData != nullptr);

	if (!IsValidCurrentShader())
		return;

	CxbxUpdateActiveIndexBuffer(pIndexData, DrawContext.dwVertexCount);
	CxbxVertexBufferConverter VertexBufferConverter;
	VertexBufferConverter.Apply(&DrawContext);
	if (DrawContext.XboxPrimitiveType == X_D3DPT_QUADLIST) {
		UINT uiStartIndex = 0;
		int iNumVertices = (int)DrawContext.dwVertexCount;
		// Indexed quadlist can be drawn using unpatched indexes via multiple draws of 2 'strip' triangles :
		// 4 vertices are just enough for two triangles (a fan starts with 3 vertices for 1 triangle,
		// and adds 1 triangle via 1 additional vertex)
		// This is slower (because of call-overhead) but doesn't require any index buffer patching.
		// Draw 1 quad as a 2 triangles in a fan (which both have the same winding order) :
		// Test-cases : XDK samples reaching this case are : DisplacementMap, Ripple
		// Test-case : XDK Samples (Billboard, BumpLens, DebugKeyboard, Gamepad, Lensflare, PerfTest?VolumeLight, PointSprites, Tiling, VolumeFog, VolumeSprites, etc)
		while (iNumVertices >= VERTICES_PER_QUAD) {
			// Determine highest and lowest index in use :
			INDEX16 LowIndex = pIndexData[uiStartIndex];
			INDEX16 HighIndex = LowIndex;
			for (int i = 1; i < VERTICES_PER_QUAD; i++) {
				INDEX16 Index = pIndexData[i];
				if (LowIndex > Index)
					LowIndex = Index;
				if (HighIndex < Index)
					HighIndex = Index;
			}
			// Emulate a quad by drawing each as a fan of 2 triangles
			HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitive(
				D3DPT_TRIANGLEFAN,
//{ $IFDEF DXBX_USE_D3D9 } {BaseVertexIndex = }0, { $ENDIF }
				LowIndex, // minIndex
				HighIndex - LowIndex + 1, // NumVertices
				uiStartIndex,
				TRIANGLES_PER_QUAD // primCount = Draw 2 triangles
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitive(X_D3DPT_QUADLIST)");

			uiStartIndex += VERTICES_PER_QUAD;
			iNumVertices -= VERTICES_PER_QUAD;
			g_dwPrimPerFrame += TRIANGLES_PER_QUAD;
		}
	}
	else {
		// Primitives other than X_D3DPT_QUADLIST can be drawn using one DrawIndexedPrimitive call :
		HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitive(
			EmuXB2PC_D3DPrimitiveType(DrawContext.XboxPrimitiveType),
			/* MinVertexIndex = */0,
			/* NumVertices = */DrawContext.dwVertexCount, // TODO : g_EmuD3DActiveStreamSizes[0], // Note : ATI drivers are especially picky about this -
			// NumVertices should be the span of covered vertices in the active vertex buffer (TODO : Is stream 0 correct?)
			DrawContext.dwStartVertex,
			DrawContext.dwHostPrimitiveCount);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitive");

		g_dwPrimPerFrame += DrawContext.dwHostPrimitiveCount;
		if (DrawContext.XboxPrimitiveType == X_D3DPT_LINELOOP) {
			// Close line-loops using a final single line, drawn from the end to the start vertex
			LOG_TEST_CASE("X_D3DPT_LINELOOP");
			// Read the end and start index from the supplied index data
			INDEX16 LowIndex = pIndexData[0];
			INDEX16 HighIndex = pIndexData[DrawContext.dwHostPrimitiveCount];
			// If needed, swap so highest index is higher than lowest (duh)
			if (HighIndex < LowIndex) {
				HighIndex ^= LowIndex;
				LowIndex ^= HighIndex;
				HighIndex ^= LowIndex;
			}

			// Draw the closing line using a helper function (which will SetIndices)
			CxbxDrawIndexedClosingLine(LowIndex, HighIndex);
			// NOTE : We don't restore the previously active index buffer
		}
	}

	VertexBufferConverter.Restore();
}

// TODO : Move to own file
// Drawing function specifically for rendering Xbox draw calls supplying a 'User Pointer'.
// Called by D3DDevice_DrawVerticesUP, EmuExecutePushBufferRaw and EmuFlushIVB
void XTL::CxbxDrawPrimitiveUP(CxbxDrawContext &DrawContext)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	assert(DrawContext.dwStartVertex == 0);
	assert(DrawContext.pXboxVertexStreamZeroData != NULL);
	assert(DrawContext.uiXboxVertexStreamZeroStride > 0);

	CxbxVertexBufferConverter VertexBufferConverter;
	VertexBufferConverter.Apply(&DrawContext);
	if (DrawContext.XboxPrimitiveType == X_D3DPT_QUADLIST) {
		// LOG_TEST_CASE("X_D3DPT_QUADLIST"); // X-Marbles and XDK Sample PlayField hits this case
		// Draw quadlists using a single 'quad-to-triangle mapping' index buffer :
		INDEX16 *pIndexData = CxbxAssureQuadListIndexBuffer(DrawContext.dwVertexCount);
		// Convert quad vertex-count to triangle vertex count :
		UINT PrimitiveCount = DrawContext.dwHostPrimitiveCount * TRIANGLES_PER_QUAD;
		HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitiveUP(
			D3DPT_TRIANGLELIST, // Draw indexed triangles instead of quads
			0, // MinVertexIndex
			DrawContext.dwVertexCount, // NumVertexIndices
			PrimitiveCount,
			pIndexData,
			D3DFMT_INDEX16,
			DrawContext.pHostVertexStreamZeroData,
			DrawContext.uiHostVertexStreamZeroStride
		);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitieUP(X_D3DPT_QUADLIST)");

		g_dwPrimPerFrame += PrimitiveCount;
	}
	else {
		// Primitives other than X_D3DPT_QUADLIST can be drawn using one DrawPrimitiveUP call :
		HRESULT hRet = g_pD3DDevice8->DrawPrimitiveUP(
			EmuXB2PC_D3DPrimitiveType(DrawContext.XboxPrimitiveType),
			DrawContext.dwHostPrimitiveCount,
			DrawContext.pHostVertexStreamZeroData,
			DrawContext.uiHostVertexStreamZeroStride
		);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawPrimitiveUP");

		g_dwPrimPerFrame += DrawContext.dwHostPrimitiveCount;
		if (DrawContext.XboxPrimitiveType == X_D3DPT_LINELOOP) {
			// Note : XDK samples reaching this case : DebugKeyboard, Gamepad, Tiling, ShadowBuffer
			// Since we can use pHostVertexStreamZeroData here, we can close the line simpler than
			// via CxbxDrawIndexedClosingLine, by drawing two indices via DrawIndexedPrimitiveUP.
			// (This is simpler because we use just indices and don't need to copy the vertices.)
			// Close line-loops using a final single line, drawn from the end to the start vertex :
			CxbxDrawIndexedClosingLineUP(
				(INDEX16)0, // LowIndex
				(INDEX16)DrawContext.dwHostPrimitiveCount, // HighIndex,
				DrawContext.pHostVertexStreamZeroData,
				DrawContext.uiHostVertexStreamZeroStride
			);
		}
	}

	VertexBufferConverter.Restore();
}

// ******************************************************************
// * patch: D3DDevice_DrawVertices
// ******************************************************************
VOID WINAPI XTL::EMUPATCH(D3DDevice_DrawVertices)
(
    X_D3DPRIMITIVETYPE PrimitiveType,
    UINT               StartVertex,
    UINT               VertexCount
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(PrimitiveType)
		LOG_FUNC_ARG(StartVertex)
		LOG_FUNC_ARG(VertexCount)
		LOG_FUNC_END;

	// Dxbx Note : In DrawVertices and DrawIndexedVertices, PrimitiveType may not be D3DPT_POLYGON

	if (!EmuD3DValidVertexCount(PrimitiveType, VertexCount)) {
		LOG_TEST_CASE("Invalid VertexCount");
		return;
	}

	// TODO : Call unpatched D3DDevice_SetStateVB(0);

	CxbxUpdateNativeD3DResources();
    #ifdef _DEBUG_TRACK_VB
    if(!g_bVBSkipStream)
    #endif
    if(IsValidCurrentShader())
    {
		CxbxDrawContext DrawContext = {};
		DrawContext.XboxPrimitiveType = PrimitiveType;
		DrawContext.dwVertexCount = VertexCount;
		DrawContext.dwStartVertex = StartVertex;
		DrawContext.hVertexShader = g_CurrentVertexShader;
		if (StartVertex > 0) {
			// LOG_TEST_CASE("StartVertex > 0"); // Test case : XDK Sample (PlayField)
			DrawContext.dwStartVertex = StartVertex; // Breakpoint location for testing. 
		}

		CxbxVertexBufferConverter VertexBufferConverter;
		VertexBufferConverter.Apply(&DrawContext);
		if (DrawContext.XboxPrimitiveType == X_D3DPT_QUADLIST) {
			// LOG_TEST_CASE("X_D3DPT_QUADLIST"); // ?X-Marbles and XDK Sample (Cartoon, ?maybe PlayField?) hits this case
			// Draw quadlists using a single 'quad-to-triangle mapping' index buffer :
			// Assure & activate that special index buffer :
			CxbxAssureQuadListD3DIndexBuffer(/*NrOfQuadVertices=*/DrawContext.dwVertexCount);
			// Convert quad vertex-count & start to triangle vertex count & start :
			UINT startIndex = QuadToTriangleVertexCount(DrawContext.dwStartVertex);
			UINT primCount = DrawContext.dwHostPrimitiveCount * TRIANGLES_PER_QUAD;
			// Determine highest and lowest index in use :
			INDEX16 LowIndex = pQuadToTriangleIndexBuffer[startIndex];
			INDEX16 HighIndex = LowIndex + (INDEX16)DrawContext.dwVertexCount - 1;
			// Emulate a quad by drawing each as a fan of 2 triangles
			HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitive(
				D3DPT_TRIANGLELIST, // Draw indexed triangles instead of quads
#ifdef DXBX_USE_D3D9 
				0, // BaseVertexIndex
#endif
				LowIndex, // minIndex
				HighIndex - LowIndex + 1, // NumVertices,
				startIndex,
				primCount
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitive(X_D3DPT_QUADLIST)");

			g_dwPrimPerFrame += primCount;
		}
		else {
			HRESULT hRet = g_pD3DDevice8->DrawPrimitive(
				EmuXB2PC_D3DPrimitiveType(DrawContext.XboxPrimitiveType),
				DrawContext.dwStartVertex,
				DrawContext.dwHostPrimitiveCount
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawPrimitive");

			g_dwPrimPerFrame += DrawContext.dwHostPrimitiveCount;
			if (DrawContext.XboxPrimitiveType == X_D3DPT_LINELOOP) {
				// Close line-loops using a final single line, drawn from the end to the start vertex
				LOG_TEST_CASE("X_D3DPT_LINELOOP"); // TODO : Text-cases needed
				INDEX16 LowIndex = (INDEX16)DrawContext.dwStartVertex;
				INDEX16 HighIndex = (INDEX16)(DrawContext.dwStartVertex + DrawContext.dwHostPrimitiveCount);
				// Draw the closing line using a helper function (which will SetIndices)
				CxbxDrawIndexedClosingLine(LowIndex, HighIndex);
				// NOTE : We don't restore the previously active index buffer
			}
		}

		VertexBufferConverter.Restore();
    }

	// Execute callback procedure
	if (g_CallbackType == X_D3DCALLBACK_WRITE) {
		if (g_pCallback != NULL) {			
			g_pCallback(g_CallbackParam);
			// TODO: Reset pointer?
		}
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_DrawVerticesUP)
(
    X_D3DPRIMITIVETYPE  PrimitiveType,
    UINT                VertexCount,
    CONST PVOID         pVertexStreamZeroData,
    UINT                VertexStreamZeroStride
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(PrimitiveType)
		LOG_FUNC_ARG(VertexCount)
		LOG_FUNC_ARG(pVertexStreamZeroData)
		LOG_FUNC_ARG(VertexStreamZeroStride)
		LOG_FUNC_END;

	if (!EmuD3DValidVertexCount(PrimitiveType, VertexCount)) {
		LOG_TEST_CASE("Invalid VertexCount");
		return;
	}

	// TODO : Call unpatched D3DDevice_SetStateUP();

	CxbxUpdateNativeD3DResources();
    #ifdef _DEBUG_TRACK_VB
    if(!g_bVBSkipStream)
    #endif
    if (IsValidCurrentShader()) {
		CxbxDrawContext DrawContext = {};
		DrawContext.XboxPrimitiveType = PrimitiveType;
		DrawContext.dwVertexCount = VertexCount;
		DrawContext.pXboxVertexStreamZeroData = pVertexStreamZeroData;
		DrawContext.uiXboxVertexStreamZeroStride = VertexStreamZeroStride;
		DrawContext.hVertexShader = g_CurrentVertexShader;

		CxbxDrawPrimitiveUP(DrawContext);
    }

	// Execute callback procedure
	if (g_CallbackType == X_D3DCALLBACK_WRITE) {
		if (g_pCallback != NULL) {
			g_pCallback(g_CallbackParam);
			// TODO: Reset pointer?
		}
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_DrawIndexedVertices)
(
    X_D3DPRIMITIVETYPE  PrimitiveType,
    UINT                VertexCount,
    CONST PWORD         pIndexData
)
{
	FUNC_EXPORTS

	// Test-cases : XDK samples (Cartoon, Gamepad)
	// Note : In gamepad.xbe, the gamepad is drawn by D3DDevice_DrawIndexedVertices
	// Dxbx Note : In DrawVertices and DrawIndexedVertices, PrimitiveType may not be D3DPT_POLYGON

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(PrimitiveType)
		LOG_FUNC_ARG(VertexCount)
		LOG_FUNC_ARG(pIndexData)
		LOG_FUNC_END;

	if (!EmuD3DValidVertexCount(PrimitiveType, VertexCount)) {
		LOG_TEST_CASE("Invalid VertexCount");
		return;
	}

	// TODO : Call unpatched D3DDevice_SetStateVB(0);

	CxbxUpdateNativeD3DResources();
    #ifdef _DEBUG_TRACK_VB
    if(!g_bVBSkipStream)
    #endif
    if (IsValidCurrentShader()) {
		CxbxDrawContext DrawContext = {};
		DrawContext.XboxPrimitiveType = PrimitiveType;
		DrawContext.dwVertexCount = VertexCount;
		DrawContext.hVertexShader = g_CurrentVertexShader;

		CxbxDrawIndexed(DrawContext, (INDEX16*)pIndexData);
	}

	// Execute callback procedure
	if (g_CallbackType == X_D3DCALLBACK_WRITE) {
		if (g_pCallback != NULL) {
			g_pCallback(g_CallbackParam);
			// TODO: Reset pointer?
		}
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_DrawIndexedVerticesUP)
(
    X_D3DPRIMITIVETYPE  PrimitiveType,
    UINT                VertexCount,
    CONST PVOID         pIndexData,
    CONST PVOID         pVertexStreamZeroData,
    UINT                VertexStreamZeroStride
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(PrimitiveType)
		LOG_FUNC_ARG(VertexCount)
		LOG_FUNC_ARG(pIndexData)
		LOG_FUNC_ARG(pVertexStreamZeroData)
		LOG_FUNC_ARG(VertexStreamZeroStride)
		LOG_FUNC_END;

	if (!EmuD3DValidVertexCount(PrimitiveType, VertexCount)) {
		LOG_TEST_CASE("Invalid VertexCount");
		return;
	}

	// TODO : Call unpatched D3DDevice_SetStateUP();

	CxbxUpdateNativeD3DResources();
	// CxbxUpdateActiveIndexBuffer() not needed (all draw calls below use pIndexData)
    #ifdef _DEBUG_TRACK_VB
    if(!g_bVBSkipStream)
    #endif
	if (IsValidCurrentShader()) {
		CxbxDrawContext DrawContext = {};
		DrawContext.XboxPrimitiveType = PrimitiveType;
		DrawContext.dwVertexCount = VertexCount;
		DrawContext.pXboxVertexStreamZeroData = pVertexStreamZeroData;
		DrawContext.uiXboxVertexStreamZeroStride = VertexStreamZeroStride;
		DrawContext.hVertexShader = g_CurrentVertexShader;

		CxbxVertexBufferConverter VertexBufferConverter;
		VertexBufferConverter.Apply(&DrawContext);
		if (DrawContext.XboxPrimitiveType == X_D3DPT_QUADLIST) {
			// Indexed quadlist can be drawn using unpatched indexes via multiple draws of 2 'strip' triangles :
			// Those 4 vertices are just enough for two triangles (a fan starts with 3 vertices for 1 triangle,
			// and adds 1 triangle via 1 additional vertex)
			// This is slower (because of call-overhead) but doesn't require any index buffer patching

			// Draw 1 quad as a 2 triangles in a fan (which both have the same winding order) :
			LOG_TEST_CASE("X_D3DPT_QUADLIST"); // Test-case : Buffy: The Vampire Slayer, FastLoad XDK Sample
			INDEX16* pWalkIndexData = (INDEX16*)pIndexData;
			int iNumVertices = (int)VertexCount;
			while (iNumVertices >= VERTICES_PER_QUAD) {
				// Determine highest and lowest index in use :
				INDEX16 LowIndex = pWalkIndexData[0];
				INDEX16 HighIndex = LowIndex;
				for (int i = 1; i < VERTICES_PER_QUAD; i++) {
					INDEX16 Index = pWalkIndexData[i];
					if (LowIndex > Index)
						LowIndex = Index;
					if (HighIndex < Index)
						HighIndex = Index;
				}
				// Emulate a quad by drawing each as a fan of 2 triangles
				HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitiveUP(
					D3DPT_TRIANGLEFAN, // Draw a triangle-fan instead of a quad
					LowIndex, // MinVertexIndex
					HighIndex - LowIndex + 1, // NumVertexIndices
					TRIANGLES_PER_QUAD, // primCount = Draw 2 triangles
					pWalkIndexData,
					D3DFMT_INDEX16,
					DrawContext.pHostVertexStreamZeroData,
					DrawContext.uiHostVertexStreamZeroStride
				);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitiveUP(X_D3DPT_QUADLIST)");

				pWalkIndexData += VERTICES_PER_QUAD;
				iNumVertices -= VERTICES_PER_QUAD;
			}

			g_dwPrimPerFrame += VertexCount / VERTICES_PER_QUAD * TRIANGLES_PER_QUAD;
		}
		else {
			LOG_TEST_CASE("DrawIndexedPrimitiveUP"); // Test-case : Burnout, Namco Museum 50th Anniversary
			HRESULT hRet = g_pD3DDevice8->DrawIndexedPrimitiveUP(
				EmuXB2PC_D3DPrimitiveType(DrawContext.XboxPrimitiveType),
				0, // MinVertexIndex
				DrawContext.dwVertexCount, // NumVertexIndices
				DrawContext.dwHostPrimitiveCount,
				pIndexData,
				D3DFMT_INDEX16,
				DrawContext.pHostVertexStreamZeroData,
				DrawContext.uiHostVertexStreamZeroStride
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawIndexedPrimitiveUP");

			g_dwPrimPerFrame += DrawContext.dwHostPrimitiveCount;
			if (DrawContext.XboxPrimitiveType == X_D3DPT_LINELOOP) {
				// Close line-loops using a final single line, drawn from the end to the start vertex
				LOG_TEST_CASE("X_D3DPT_LINELOOP"); // TODO : Which titles reach this case?
				// Read the end and start index from the supplied index data
				INDEX16 LowIndex = ((INDEX16*)pIndexData)[0];
				INDEX16 HighIndex = ((INDEX16*)pIndexData)[DrawContext.dwHostPrimitiveCount];
				// If needed, swap so highest index is higher than lowest (duh)
				if (HighIndex < LowIndex) {
					HighIndex ^= LowIndex;
					LowIndex ^= HighIndex;
					HighIndex ^= LowIndex;
				}

				// Close line-loops using a final single line, drawn from the end to the start vertex :
				CxbxDrawIndexedClosingLineUP(
					LowIndex,
					HighIndex,
					DrawContext.pHostVertexStreamZeroData,
					DrawContext.uiHostVertexStreamZeroStride
				);
			}
		}

	    VertexBufferConverter.Restore();
	}

	// Execute callback procedure
	if (g_CallbackType == X_D3DCALLBACK_WRITE) {
		if (g_pCallback != NULL) {
			g_pCallback(g_CallbackParam);
			// TODO: Reset pointer?
		}
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetLight)
(
    DWORD            Index,
	X_D3DLIGHT      *pLight
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(pLight)
		LOG_FUNC_END;

    HRESULT hRet = g_pD3DDevice8->GetLight(Index, pLight);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetLight");    
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_SetLight)
(
    DWORD            Index,
    CONST X_D3DLIGHT *pLight
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(pLight)
		LOG_FUNC_END;

    HRESULT hRet = g_pD3DDevice8->SetLight(Index, pLight);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetLight");    

    return hRet;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetMaterial)
(
	X_D3DMATERIAL* pMaterial
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pMaterial);

	if (pMaterial)
	{
		HRESULT hRet = g_pD3DDevice8->GetMaterial(pMaterial);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetMaterial");
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetMaterial)
(
    CONST X_D3DMATERIAL *pMaterial
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pMaterial);

    HRESULT hRet = g_pD3DDevice8->SetMaterial(pMaterial);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetMaterial");
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_LightEnable)
(
    DWORD            Index,
    BOOL             bEnable
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(bEnable)
		LOG_FUNC_END;

    HRESULT hRet = g_pD3DDevice8->LightEnable(Index, bEnable);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->LightEnable");    

    return hRet;
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetLightEnable)
(
	DWORD            Index,
	BOOL             *bEnable
)
{
	FUNC_EXPORTS

		LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Index)
		LOG_FUNC_ARG(bEnable)
		LOG_FUNC_END;

	HRESULT hRet = g_pD3DDevice8->GetLightEnable(Index, bEnable);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetLightEnable");

	return hRet;
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderTarget)
(
    X_D3DSurface    *pRenderTarget,
    X_D3DSurface    *pNewZStencil
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pRenderTarget)
		LOG_FUNC_ARG(pNewZStencil)
		LOG_FUNC_END;

	if (pRenderTarget != NULL)
		g_pActiveXboxRenderTarget = pRenderTarget;

	g_pActiveXboxDepthStencil = pNewZStencil;

	UpdateDepthStencilFlags(pNewZStencil);

    // TODO: Follow that stencil!
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreatePalette)
(
    X_D3DPALETTESIZE    Size,
    X_D3DPalette      **ppPalette
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_CreatePalette2");

    *ppPalette = EMUPATCH(D3DDevice_CreatePalette2)(Size);

    return D3D_OK;
}
#endif

#if 0 // Patch disabled
int XboxD3DPaletteSizeToBytes(const XTL::X_D3DPALETTESIZE Size)
{
	static int lk[4] =
	{
		256 * sizeof(XTL::D3DCOLOR),    // D3DPALETTE_256
		128 * sizeof(XTL::D3DCOLOR),    // D3DPALETTE_128
		64 * sizeof(XTL::D3DCOLOR),     // D3DPALETTE_64
		32 * sizeof(XTL::D3DCOLOR)      // D3DPALETTE_32
	};

	return lk[Size];
}

XTL::X_D3DPalette * WINAPI XTL::EMUPATCH(D3DDevice_CreatePalette2)
(
    X_D3DPALETTESIZE    Size
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Size);

    X_D3DPalette *pPalette = EmuNewD3DPalette();

	pPalette->Common |= (Size << X_D3DPALETTE_COMMON_PALETTESIZE_SHIFT);
    pPalette->Data = (DWORD)g_MemoryManager.AllocateContiguous(XboxD3DPaletteSizeToBytes(Size), PAGE_SIZE);

    DbgPrintf("pPalette: = 0x%.08X\n", pPalette);

    RETURN(pPalette);
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetPalette)
(
    DWORD         Stage,
    X_D3DPalette *pPalette
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Stage)
		LOG_FUNC_ARG(pPalette)
		LOG_FUNC_END;

	//    g_pD3DDevice9->SetPaletteEntries(Stage?, (PALETTEENTRY*)pPalette->Data);
	//    g_pD3DDevice9->SetCurrentTexturePalette(Stage, Stage);

	if (Stage < X_D3DTSS_STAGECOUNT)
		// Cache palette data and size
		g_pTexturePaletteStages[Stage] = (DWORD *)GetDataFromXboxResource(pPalette);
}
#endif

void WINAPI XTL::EMUPATCH(D3DDevice_SetFlickerFilter)
(
    DWORD         Filter
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Filter);

	LOG_IGNORED();
}

void WINAPI XTL::EMUPATCH(D3DDevice_SetSoftDisplayFilter)
(
    BOOL Enable
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Enable);

	LOG_IGNORED();
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DPalette_Lock)
(
    X_D3DPalette   *pThis,
    D3DCOLOR      **ppColors,
    DWORD           Flags
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DPalette_Lock2");

	HRESULT hRet = D3D_OK;

	if( pThis != NULL)
		*ppColors = EMUPATCH(D3DPalette_Lock2)(pThis, Flags);
	else
	{
		EmuWarning( "EmuIDirect3DPalette8_Lock: pThis == NULL!" );
		hRet = E_FAIL;
	}
}
#endif

#if 0 // Patch disabled
XTL::D3DCOLOR * WINAPI XTL::EMUPATCH(D3DPalette_Lock2)
(
    X_D3DPalette   *pThis,
    DWORD           Flags
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    // If X_D3DLOCK_READONLY and X_D3DLOCK_NOOVERWRITE bitflags not set
    if( !(Flags & (X_D3DLOCK_READONLY | X_D3DLOCK_NOOVERWRITE)) )
    {
		EMUPATCH(D3DResource_BlockUntilNotBusy)(pThis);
    }

    D3DCOLOR *pColors = (D3DCOLOR*)pThis->Data;

    RETURN(pColors);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderSize)
(
    DWORD Handle,
    UINT* pSize
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pSize)
		LOG_FUNC_END;

	if (pSize != NULL) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		if (pHostVertexShader != nullptr) {
			*pSize = pHostVertexShader->Size;
		}
		else {
			*pSize = 0;
		}
	}
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_DeleteVertexShader)
(
    DWORD Handle
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Handle);

    DWORD HostVertexHandle = 0;

    if (VshHandleIsVertexShader(Handle)) {
		X_D3DVertexShader *pXboxVertexShader = VshHandleGetXboxVertexShader(Handle);
		CxbxVertexShader *pHostVertexShader = GetHostVertexShader(pXboxVertexShader);

        HostVertexHandle = pHostVertexShader->Handle;
		if (pHostVertexShader->pDeclaration != nullptr) {
			g_MemoryManager.Free(pHostVertexShader->pDeclaration);
		}

        if (pHostVertexShader->pFunction != nullptr) {
            g_MemoryManager.Free(pHostVertexShader->pFunction);
        }

        FreeVertexDynamicPatch(pHostVertexShader);

        g_MemoryManager.Free(pHostVertexShader);
        g_MemoryManager.Free(pXboxVertexShader);
    }

    HRESULT hRet = g_pD3DDevice8->DeleteVertexShader(HostVertexHandle);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DeleteVertexShader");    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SelectVertexShaderDirect)
(
    X_VERTEXATTRIBUTEFORMAT *pVAF,
    DWORD                    Address
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pVAF)
		LOG_FUNC_ARG(Address)
		LOG_FUNC_END;

    LOG_UNIMPLEMENTED(); 
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetShaderConstantMode)
(
    DWORD *pMode
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pMode);
        
    if(pMode)
    {
        *pMode = g_VertexShaderConstantMode;
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShader)
(
    DWORD *pHandle
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pHandle);

    if(pHandle)
    {
        (*pHandle) = g_CurrentVertexShader;
    }
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderConstant)
(
    INT   Register,
    void  *pConstantData,
    DWORD ConstantCount
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Register)
		LOG_FUNC_ARG(pConstantData)
		LOG_FUNC_ARG(ConstantCount)
		LOG_FUNC_END;

	// TODO -oDxbx: If we ever find a title that calls this, check if this correction
	// should indeed be done version-dependantly (like in SetVertexShaderConstant);
	// It seems logical that these two mirror eachother, but it could well be different:
	if (X_D3DSCM_CORRECTION_VersionDependent > 0) {
		Register += X_D3DSCM_CORRECTION_VersionDependent;
		DbgPrintf("Corrected constant register : 0x%.08x\n", Register);
	}

    HRESULT hRet = g_pD3DDevice8->GetVertexShaderConstant
    (
        Register,
        pConstantData,
        ConstantCount
    );

	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetVertexShaderConstant");    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexShaderInputDirect)
(
    X_VERTEXATTRIBUTEFORMAT *pVAF,
    UINT                     StreamCount,
    X_STREAMINPUT           *pStreamInputs
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pVAF)
		LOG_FUNC_ARG(StreamCount)
		LOG_FUNC_ARG(pStreamInputs)
		LOG_FUNC_END;

    LOG_UNIMPLEMENTED(); 
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderInput)
(
    DWORD              *pHandle,
    UINT               *pStreamCount,
    X_STREAMINPUT      *pStreamInputs
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pHandle)
		LOG_FUNC_ARG(pStreamCount)
		LOG_FUNC_ARG(pStreamInputs)
		LOG_FUNC_END;

    LOG_UNIMPLEMENTED(); 

    return 0;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetVertexShaderInput)
(
    DWORD              Handle,
    UINT               StreamCount,
    X_STREAMINPUT     *pStreamInputs
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(StreamCount)
		LOG_FUNC_ARG(pStreamInputs)
		LOG_FUNC_END;

    LOG_UNIMPLEMENTED(); 
}


VOID WINAPI XTL::EMUPATCH(D3DDevice_RunVertexStateShader)
(
    DWORD Address,
    CONST FLOAT *pData
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Address)
		LOG_FUNC_ARG(pData)
		LOG_FUNC_END;

    LOG_UNIMPLEMENTED(); 
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_LoadVertexShaderProgram)
(
    CONST DWORD *pFunction,
    DWORD        Address
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pFunction)
		LOG_FUNC_ARG(Address)
		LOG_FUNC_END;

	LOG_UNIMPLEMENTED();    
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderType)
(
    DWORD  Handle,
    DWORD *pType
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pType)
		LOG_FUNC_END;

    if (pType != NULL) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		if (pHostVertexShader != nullptr) {
			*pType = pHostVertexShader->Type;
		}
    }
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderDeclaration)
(
    DWORD  Handle,
    PVOID  pData,
    DWORD *pSizeOfData
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pData)
		LOG_FUNC_ARG(pSizeOfData)
		LOG_FUNC_END;

    HRESULT hRet = D3DERR_INVALIDCALL;

    if (pSizeOfData != NULL) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		if (pHostVertexShader != nullptr) {
			if (*pSizeOfData < pHostVertexShader->DeclarationSize || (pData == NULL)) {
				*pSizeOfData = pHostVertexShader->DeclarationSize;
				hRet = (pData == NULL) ? D3D_OK : D3DERR_MOREDATA;
			}
			else {
				memcpy(pData, pHostVertexShader->pDeclaration, pHostVertexShader->DeclarationSize);
				hRet = D3D_OK;
			}
		}
    }

    RETURN(hRet);
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetVertexShaderFunction)
(
    DWORD  Handle,
    PVOID *pData,
    DWORD *pSizeOfData
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pData)
		LOG_FUNC_ARG(pSizeOfData)
		LOG_FUNC_END;

    HRESULT hRet = D3DERR_INVALIDCALL;

    if (pSizeOfData != NULL) {
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(Handle);
		if (pHostVertexShader != nullptr) {
			if (*pSizeOfData < pHostVertexShader->FunctionSize || pData == NULL) {
				*pSizeOfData = pHostVertexShader->FunctionSize;
				hRet = (pData == NULL) ? D3D_OK : D3DERR_MOREDATA;
			}
			else {
				memcpy(pData, pHostVertexShader->pFunction, pHostVertexShader->FunctionSize);
				hRet = D3D_OK;
			}
		}
    }
    
    RETURN(hRet);
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_SetDepthClipPlanes)
(
    FLOAT Near,
    FLOAT Far,
    DWORD Flags
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Near)
		LOG_FUNC_ARG(Far)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;

    switch(Flags) // Member of X_D3DSET_DEPTH_CLIP_PLANES_FLAGS enum
    {
        case X_D3DSDCP_SET_VERTEXPROGRAM_PLANES:
        {
            // Sets the depth-clipping planes used whenever vertex shader programs are active
            // TODO

            // pDevice->fNear = Near
            // pDevice->fFar  = Far
        }
        break;

        case X_D3DSDCP_SET_FIXEDFUNCTION_PLANES:
        {
            // Sets the depth-clipping planes used whenever the fixed-function pipeline is in use. 
            // TODO

            // pDevice->fNear = Near
            // pDevice->fFar  = Far
        }
        break;

        case X_D3DSDCP_USE_DEFAULT_VERTEXPROGRAM_PLANES:
        {
            // Causes Direct3D to disregard the depth-clipping planes set when using X_D3DSDCP_SET_VERTEXPROGRAM_PLANE. 
            // Direct3D will resume using its own internally calculated clip planes when vertex shader programs are active. 
            // TODO
        }
        break;

        case X_D3DSDCP_USE_DEFAULT_FIXEDFUNCTION_PLANES:
        {
            // Causes Direct3D to disregard the depth-clipping planes set when using X_D3DSDCP_SET_FIXEDFUNCTION_PLANES. 
            // Direct3D will resume using its own internally calculated clip planes when the fixed-function pipeline is active.
            // TODO
        }
        break;

        default:
            EmuWarning("Unknown SetDepthClipPlanes Flags provided");;
    }

    // TODO

    

    return hRet;
}

#if 0 // Patch disabled (Just calls MmAllocateContiguousMemory)
PVOID WINAPI XTL::EMUPATCH(D3D_AllocContiguousMemory)
(
    SIZE_T dwSize,
    DWORD dwAllocAttributes
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(dwSize)
		LOG_FUNC_ARG(dwAllocAttributes)
		LOG_FUNC_END;

    //
    // NOTE: Kludgey (but necessary) solution:
    //
    // Since this memory must be aligned on a page boundary, we must allocate an extra page
    // so that we can return a valid page aligned pointer
    //

    PVOID pRet = g_MemoryManager.Allocate(dwSize + PAGE_SIZE);

    // align to page boundary
    {
        DWORD dwRet = (DWORD)pRet;

        dwRet += PAGE_SIZE - dwRet % PAGE_SIZE;

        g_AlignCache.insert(dwRet, pRet);

        pRet = (PVOID)dwRet;
    }

	RETURN(pRet);
}
#endif

#if 0 // Patch disabled (Just calls Get2DSurfaceDesc)
// ******************************************************************
// * patch: IDirect3DTexture8_GetLevelDesc
// ******************************************************************
HRESULT WINAPI XTL::EMUPATCH(D3DTexture_GetLevelDesc)
(
    UINT Level,
    X_D3DSURFACE_DESC* pDesc
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(pDesc)
		LOG_FUNC_END;    

    return D3D_OK;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(Direct3D_CheckDeviceMultiSampleType)
(
    UINT                 Adapter,
    X_D3DDEVTYPE         DeviceType,
    X_D3DFORMAT          SurfaceFormat,
    BOOL                 Windowed,
    X_D3DMULTISAMPLE_TYPE  MultiSampleType
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Adapter)
		LOG_FUNC_ARG(DeviceType)
		LOG_FUNC_ARG(SurfaceFormat)
		LOG_FUNC_ARG(Windowed)
		LOG_FUNC_ARG(MultiSampleType)
		LOG_FUNC_END;

    if(Adapter != D3DADAPTER_DEFAULT)
    {
        EmuWarning("Adapter is not D3DADAPTER_DEFAULT, correcting!");
        Adapter = D3DADAPTER_DEFAULT;
    }

    if(DeviceType == D3DDEVTYPE_FORCE_DWORD)
        EmuWarning("DeviceType == D3DDEVTYPE_FORCE_DWORD");

    // Convert SurfaceFormat (Xbox->PC)
    D3DFORMAT PCSurfaceFormat = EmuXB2PC_D3DFormat(SurfaceFormat);

    // TODO: HACK: Devices that don't support this should somehow emulate it!
    if(PCSurfaceFormat == D3DFMT_D16)
    {
        EmuWarning("D3DFMT_D16 is an unsupported texture format!");
        PCSurfaceFormat = D3DFMT_X8R8G8B8;
    }
    else if(PCSurfaceFormat == D3DFMT_P8 && !IsResourceFormatSupported(X_D3DRTYPE_SURFACE, D3DFMT_P8, 0))
    {
        EmuWarning("D3DFMT_P8 is an unsupported texture format!");
        PCSurfaceFormat = D3DFMT_X8R8G8B8;
    }
    else if(PCSurfaceFormat == D3DFMT_D24S8)
    {
        EmuWarning("D3DFMT_D24S8 is an unsupported texture format!");
        PCSurfaceFormat = D3DFMT_X8R8G8B8;
    }

    if(Windowed != FALSE)
        Windowed = FALSE;

    // TODO: Convert from Xbox to PC!!
    D3DMULTISAMPLE_TYPE PCMultiSampleType = EmuXB2PC_D3DMULTISAMPLE_TYPE(MultiSampleType);

    // Now call the real CheckDeviceMultiSampleType with the corrected parameters.
    HRESULT hRet = g_pD3D8->CheckDeviceMultiSampleType
    (
        Adapter,
        DeviceType,
        PCSurfaceFormat,
        Windowed,
        PCMultiSampleType
    );

    

    return hRet;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3D_GetDeviceCaps) // TODO : Rename to Direct3D_GetDeviceCaps
(
    UINT        Adapter,
    X_D3DDEVTYPE  DeviceType,
    X_D3DCAPS   *pCaps
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Adapter)
		LOG_FUNC_ARG(DeviceType)
		LOG_FUNC_ARG(pCaps)
		LOG_FUNC_END;

    HRESULT hRet = g_pD3D8->GetDeviceCaps(Adapter, DeviceType, pCaps);
	DEBUG_D3DRESULT(hRet, "g_pD3D8->GetDeviceCaps");

	if(FAILED(hRet))
		CxbxKrnlCleanup("IDirect3D8::GetDeviceCaps failed!");    

    return hRet;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3D_SetPushBufferSize) // TODO : Rename to Direct3D_SetPushBufferSize
(
    DWORD PushBufferSize,
    DWORD KickOffSize
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(PushBufferSize)
		LOG_FUNC_ARG(KickOffSize)
		LOG_FUNC_END;

    HRESULT hRet = D3D_OK;

    // This is a Xbox extension, meaning there is no pc counterpart.

    

    return hRet;
}
#endif

DWORD WINAPI XTL::EMUPATCH(D3DDevice_InsertFence)()
{
	FUNC_EXPORTS

	LOG_FUNC();

    // TODO: Actually implement this
    DWORD dwRet = 0x8000BEEF;

	LOG_UNIMPLEMENTED();

    return dwRet;
}

BOOL WINAPI XTL::EMUPATCH(D3DDevice_IsFencePending)
(
    DWORD Fence
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Fence);

	// TODO: Implement
	LOG_UNIMPLEMENTED();

	return FALSE;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_BlockOnFence)
(
    DWORD Fence
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Fence);

    // TODO: Implement
	LOG_UNIMPLEMENTED();
}

VOID WINAPI XTL::EMUPATCH(D3DResource_BlockUntilNotBusy)
(
    X_D3DResource *pThis
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pThis);

    // TODO: Implement
	LOG_UNIMPLEMENTED();
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DVertexBuffer_GetDesc)
(
    X_D3DVertexBuffer    *pThis,
    X_D3DVERTEXBUFFER_DESC *pDesc
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(pDesc)
		LOG_FUNC_END;

	pDesc->Format = X_D3DFMT_VERTEXDATA;
	pDesc->Type = X_D3DRTYPE_VERTEXBUFFER;
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetScissors)
(
    DWORD          Count,
    BOOL           Exclusive,
    CONST D3DRECT  *pRects
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Count)
		LOG_FUNC_ARG(Exclusive)
		LOG_FUNC_ARG(pRects)
		LOG_FUNC_END;

    // TODO: Implement
	LOG_UNIMPLEMENTED();
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetScreenSpaceOffset)
(
    FLOAT x,
    FLOAT y
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(x)
		LOG_FUNC_ARG(y)
		LOG_FUNC_END;

    EmuWarning("EmuD3DDevice_SetScreenSpaceOffset ignored");
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetPixelShaderProgram)
(
	X_D3DPIXELSHADERDEF* pPSDef
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pPSDef);

	HRESULT hRet = E_FAIL;
	DWORD dwHandle;

	// Redirect this call to windows Direct3D
	hRet = g_pD3DDevice8->CreatePixelShader
    (
        (DWORD*) pPSDef,
        &dwHandle
    );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreatePixelShader");

    if(FAILED(hRet))
    {
        dwHandle = X_PIXELSHADER_FAKE_HANDLE;

        EmuWarning("We're lying about the creation of a pixel shader!");
    }

	// Now, redirect this to Xbox Direct3D 
	//
	//EMUPATCH(D3DDevice_CreatePixelShader)(pPSDef, &dwHandle);
	//hRet = EMUPATCH(D3DDevice_SetPixelShader)( dwHandle );
	//
}
#endif

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_CreateStateBlock)
(
	D3DSTATEBLOCKTYPE Type,
	DWORD			  *pToken
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Type)
		LOG_FUNC_ARG(pToken)
		LOG_FUNC_END;

	// blueshogun96 10/1/07
	// I'm assuming this is the same as the PC version...

	HRESULT hRet = g_pD3DDevice8->CreateStateBlock( Type, pToken );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateStateBlock");

	return hRet;
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_InsertCallback)
(
	X_D3DCALLBACKTYPE	Type,
	X_D3DCALLBACK		pCallback,
	DWORD				Context
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Type)
		LOG_FUNC_ARG(pCallback)
		LOG_FUNC_ARG(Context)
		LOG_FUNC_END;

	// TODO: Implement list (now it's only one)
	g_pCallback = pCallback;
	g_CallbackType = Type;
	g_CallbackParam = Context;

	LOG_INCOMPLETE();
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_DrawRectPatch)
(
	UINT					Handle,
	CONST FLOAT				*pNumSegs,
	CONST X_D3DRECTPATCH_INFO *pRectPatchInfo
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pNumSegs)
		LOG_FUNC_ARG(pRectPatchInfo)
		LOG_FUNC_END;

	CxbxUpdateNativeD3DResources();

	HRESULT hRet = g_pD3DDevice8->DrawRectPatch( Handle, pNumSegs, pRectPatchInfo );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawRectPatch");

	return hRet;
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_DrawTriPatch)
(
	UINT					Handle,
	CONST FLOAT				*pNumSegs,
	CONST X_D3DTRIPATCH_INFO* pTriPatchInfo
)
{
	FUNC_EXPORTS

		LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Handle)
		LOG_FUNC_ARG(pNumSegs)
		LOG_FUNC_ARG(pTriPatchInfo)
		LOG_FUNC_END;

	CxbxUpdateNativeD3DResources();

	HRESULT hRet = g_pD3DDevice8->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DrawTriPatch");

	return hRet;
}

#pragma warning(disable:4244)
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetProjectionViewportMatrix)
(
	X_D3DXMATRIX *pProjectionViewport
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pProjectionViewport);

	// blueshogun96 1/25/10
	// It's been almost 3 years, but I think this is a better 
	// implementation.  Still probably not right, but better
	// then before.

	HRESULT hRet;
	D3DXMATRIX Out, mtxProjection, mtxViewport;
	D3DVIEWPORT8 Viewport;

	// Get current viewport
	hRet = g_pD3DDevice8->GetViewport(&Viewport);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetViewport - Unable to get viewport!");

	// Get current projection matrix
	hRet = g_pD3DDevice8->GetTransform(D3DTS_PROJECTION, &mtxProjection);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetTransform - Unable to get projection matrix!");

	// Clear the destination matrix
	memset(Out, 0, sizeof(D3DMATRIX)); // = { 0 } crashes. Was ::ZeroMemory(&Out, sizeof(D3DMATRIX));

	// Create the Viewport matrix manually
	// Direct3D8 doesn't give me everything I need in a viewport structure
	// (one thing I REALLY HATE!) so some constants will have to be used
	// instead.

	float ClipWidth = 2.0f;
	float ClipHeight = 2.0f;
	float ClipX = -1.0f;
	float ClipY = 1.0f;
	float Width = DWtoF(Viewport.Width);
	float Height = DWtoF(Viewport.Height);

	D3DXMatrixIdentity(&mtxViewport);
	mtxViewport._11 = Width / ClipWidth;
	mtxViewport._22 = -(Height / ClipHeight);
	mtxViewport._41 = -(ClipX * mtxViewport._11);
	mtxViewport._42 = -(ClipY * mtxViewport._22);

	// Multiply projection and viewport matrix together
	Out = mtxProjection * mtxViewport;

	*pProjectionViewport = Out;

//	__asm int 3;
}
#pragma warning(default:4244)

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_KickOff)()
{
//	FUNC_EXPORTS
		
	LOG_FUNC();

	// TODO: Anything (kick off and NOT wait for idle)?
	// NOTE: We should actually emulate D3DDevice_KickPushBuffer()
	// instead of this function.  When needed, use the breakpoint (int 3)
	// to determine what is calling this function if it's something other
	// than D3DDevice_KickPushBuffer() itself.

//	__asm int 3;

	LOG_UNIMPLEMENTED();
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_KickPushBuffer)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	// TODO -oDxbx : Locate the current PushBuffer address, and supply that to RunPushBuffer (without a fixup)
	LOG_UNIMPLEMENTED();

}
#endif

#if 0 // Patch disabled
XTL::X_D3DResource* WINAPI XTL::EMUPATCH(D3DDevice_GetTexture2)
(
	DWORD Stage
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Stage);
	
	// Get the active texture from this stage
	X_D3DBaseTexture* pRet = GetXboxBaseTexture(Stage);
	if (pRet != NULL)
		pRet->Common++; // EMUPATCH(D3DResource_AddRef)(pRet) would give too much overhead (and needless logging)

	RETURN(pRet);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetStateVB)( ULONG Unknown1 )
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Unknown1);

	// TODO: Anything?
//	__asm int 3;

	LOG_UNIMPLEMENTED();	
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_SetStateUP)()
{
	FUNC_EXPORTS

	LOG_FUNC();

	LOG_UNIMPLEMENTED();

	// TODO: Anything?
//	__asm int 3;
	
}

void WINAPI XTL::EMUPATCH(D3DDevice_SetStipple)( DWORD* pPattern )
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pPattern);

	// We need an OpenGL port... badly

	LOG_IGNORED();
}

void WINAPI XTL::EMUPATCH(D3DDevice_SetSwapCallback)
(
	X_D3DSWAPCALLBACK		pCallback
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pCallback);

    g_pSwapCallback = pCallback;
}

HRESULT WINAPI XTL::EMUPATCH(D3DDevice_PersistDisplay)()
{
	FUNC_EXPORTS

	LOG_FUNC();

	HRESULT hRet = D3D_OK;

	// TODO: If this functionality is ever really needed, an idea for 
	// implementation would be to save a copy of the backbuffer's contents
	// and free the memory after the next call to D3DDevice::Present().
	// This temporary data could also be made available to the Xbox game
	// through AvGetSavedDataAddress() since D3DDevice::GetPersistedDisplay2
	// just contains a call to that kernel function.  So far, Unreal Champ-
	// ionship is the only game that uses this functionality that I know of.
	// Other Unreal Engine 2.x games might as well.

	if(!g_pD3DDevice8)
	{
		EmuWarning("Direct3D device not initialized!");
		hRet =  E_FAIL;
	}
	else
	if(g_pActiveHostBackBuffer != nullptr)
	{
		//D3DXSaveSurfaceToFile( "persisted_surface.bmp", D3DXIFF_BMP, g_pActiveHostBackBuffer, NULL, NULL );
		// DbgPrintf("Persisted display surface saved to persisted_surface.bmp\n");
	}
	else
	{
		EmuWarning("(Temporarily) Not persisting display. Blueshogun can fix this.");
	}

	/*else
	{
		IDirect3DSurface8* pBackBuffer = NULL;
		D3DLOCKED_RECT LockedRect;
		D3DSURFACE_DESC BackBufferDesc;

		g_pD3DDevice8->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
		
		pBackBuffer->GetDesc( &BackBufferDesc );

		DWORD dwBytesPerPixel = ( BackBufferDesc.Format == D3DFMT_X8R8G8B8 || BackBufferDesc.Format == D3DFMT_A8R8G8B8 ) ? 4 : 2;
		FILE* fp = fopen( "PersistedSurface.bin", "wb" );
		if(fp)
		{
			void* ptr = g_MemoryManager.Allocate( BackBufferDesc.Width * BackBufferDesc.Height * dwBytesPerPixel );

			if( SUCCEEDED( pBackBuffer->LockRect( &LockedRect, NULL, D3DLOCK_READONLY ) ) )
			{
				CopyMemory( ptr, LockedRect.pBits, BackBufferDesc.Width * BackBufferDesc.Height * dwBytesPerPixel );
				
				fwrite( ptr, BackBufferDesc.Width * BackBufferDesc.Height * dwBytesPerPixel, 1, fp );

				pBackBuffer->UnlockRect();
			}
	
			fclose(fp);
		}
	}*/

		

	return hRet;
}

#if 0 // Patch disabled, calls AvSendTVEncoderOption which calls our AvQueryAvCapabilities
DWORD WINAPI XTL::EMUPATCH(D3D_CMiniport_GetDisplayCapabilities)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	// This function was only found in Run Like Hell (5233) @ 0x11FCD0.
	// So far, this function hasn't been found in any other XDKs.  

	DWORD AvInfo;

	xboxkrnl::AvSendTVEncoderOption(NULL,
		AV_QUERY_AV_CAPABILITIES,
		0,
		&AvInfo);

	RETURN(AvInfo);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_PrimeVertexCache)
(
	UINT  VertexCount,
	WORD *pIndexData
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(VertexCount)
		LOG_FUNC_ARG(pIndexData)
		LOG_FUNC_END;

	// TODO: Implement
	LOG_UNIMPLEMENTED();
}

VOID WINAPI XTL::EMUPATCH(D3DDevice_DeleteStateBlock)
(
	DWORD Token
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Token);

	HRESULT hRet = g_pD3DDevice8->DeleteStateBlock(Token);
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->DeleteStateBlock");
}


// TODO : Disable all transform patches (D3DDevice__[Get|Set][Transform|ModelView]), once we read Xbox matrices on time
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetModelView)
(
	CONST D3DMATRIX *pModelView, 
	CONST D3DMATRIX *pInverseModelView, 
	CONST D3DMATRIX *pComposite
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pModelView)
		LOG_FUNC_ARG(pInverseModelView)
		LOG_FUNC_ARG(pComposite)
		LOG_FUNC_END;

	// TODO: Implement
	LOG_UNIMPLEMENTED();
}

void WINAPI XTL::EMUPATCH(D3DDevice_FlushVertexCache)()
{
	FUNC_EXPORTS

	LOG_FUNC();

	LOG_UNIMPLEMENTED();
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_BeginPushBuffer)
(
	X_D3DPushBuffer *pPushBuffer
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pPushBuffer);

	// TODO: Implement. Easier said than done with Direct3D, but OpenGL
	// can emulate this functionality rather easily.
	LOG_UNIMPLEMENTED();
//	CxbxKrnlCleanup("BeginPushBuffer is not yet implemented!\n"
	//				"This is going to be a difficult fix for Direct3D but NOT OpenGL!");

}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_EndPushBuffer)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	LOG_UNIMPLEMENTED();

	return D3D_OK;
}
#endif

#if 0 // Patch disabled
PDWORD WINAPI XTL::EMUPATCH(XMETAL_StartPush)(void* Unknown)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Unknown);

	// This function is too low level to actually emulate
	// Only use for debugging.
//	__asm int 3;

	LOG_UNIMPLEMENTED();

	return NULL;
}
#endif

// TODO : Disable all transform patches (D3DDevice__[Get|Set][Transform|ModelView]), once we read Xbox matrices on time
HRESULT WINAPI XTL::EMUPATCH(D3DDevice_GetModelView)
(
	X_D3DXMATRIX* pModelView
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pModelView);

	D3DXMATRIX mtxWorld, mtxView;

	// I hope this is right
	g_pD3DDevice8->GetTransform( D3DTS_WORLD, &mtxWorld );
	g_pD3DDevice8->GetTransform( D3DTS_VIEW, &mtxView );

	*pModelView = mtxWorld * mtxView;

	return D3D_OK;
}

#if 0 // Patch disabled (BackMaterial isn't emulated currently, but Xbox code is allowed to use it)
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetBackMaterial)
(
	X_D3DMATERIAL* pMaterial
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pMaterial);

	*pMaterial = g_BackMaterial;
}
#endif

#if 0 // Patch disabled (BackMaterial isn't emulated currently, but Xbox code is allowed to use it)
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetBackMaterial)
(
	X_D3DMATERIAL* pMaterial
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pMaterial);

	LOG_NOT_SUPPORTED();

	g_BackMaterial = *pMaterial;
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3D_GetAdapterIdentifier) // TODO : Rename to Direct3D_GetAdapterIdentifier
(
	UINT					Adapter,
	DWORD					Flags,
	X_D3DADAPTER_IDENTIFIER *pIdentifier
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Adapter)
		LOG_FUNC_ARG(Flags)
		LOG_FUNC_ARG(pIdentifier)
		LOG_FUNC_END;

	// TODO: Fill the Intentifier structure with the content of what an Xbox would return.
	// It might not matter for now, but just in case.

	// NOTE: Games do not crash when this function is not intercepted (at least not so far)
	// so it's recommended to add this function to every XDK you possibly can as it will
	// save you much hassle (at least it did for Max Payne).

	HRESULT hRet = g_pD3D8->GetAdapterIdentifier( Adapter, Flags, pIdentifier );
	DEBUG_D3DRESULT(hRet, "g_pD3D8->GetAdapterIdentifier");		

	return hRet;
}
#endif

#if 0 // Patch disabled

// TODO : Remove
DWORD PushBuffer[64 * 1024 / sizeof(DWORD)] = { 0 };
void DumpPushBufferContents()
{
	// TODO : Document samples that hit this
	int i = 0;
	while (PushBuffer[i] != 0)
	{
		// TODO : Convert NV2A methods to readable string, dump arguments, later on: execute commands
		DbgPrintf("PushBuffer[%4d] : 0x%X\n", i, PushBuffer[i]);
		i++;
	}

	memset(PushBuffer, 0, i * sizeof(DWORD));
}

PDWORD WINAPI XTL::EMUPATCH(MakeRequestedSpace)
(
	DWORD MinimumSpace,
	DWORD RequestedSpace
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(MinimumSpace)
		LOG_FUNC_ARG(RequestedSpace)
		LOG_FUNC_END;

	// NOTE: This function is ignored, as we currently don't emulate the push buffer
	LOG_IGNORED();

	DumpPushBufferContents();

	return PushBuffer; // Return a buffer that will be filled with GPU commands

	// Note: This should work together with functions like XMETAL_StartPush/
	// D3DDevice_BeginPush(Buffer)/D3DDevice_EndPush(Buffer) and g_pPrimaryPB

	// TODO : Once we start emulating the PushBuffer, this will have to be the
	// actual pushbuffer, for which we should let CreateDevice run unpatched.
	// Also, we will require a mechanism (thread) which handles the commands
	// send to the pushbuffer, emulating them much like EmuExecutePushBufferRaw
	// (maybe even use that).
}
#endif

#if 0 // Patch disabled
void WINAPI XTL::EMUPATCH(D3DDevice_MakeSpace)()
{
//	FUNC_EXPORTS

	LOG_FUNC();

	// NOTE: Like the above function, this should not be emulated.  The intended
	// usage is the same as above.
	LOG_UNIMPLEMENTED();
		
}
#endif

void WINAPI XTL::EMUPATCH(D3D_SetCommonDebugRegisters)() // TODO : Rename to CommonSetDebugRegisters
{
	FUNC_EXPORTS

	LOG_FUNC();

	// NOTE: I added this because I was too lazy to deal with emulating certain render
	// states that use it.  

	LOG_UNIMPLEMENTED();

}

void WINAPI XTL::EMUPATCH(D3D_BlockOnTime)( DWORD Unknown1, int Unknown2 ) // TODO : Rename to BlockOnTime
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Unknown1)
		LOG_FUNC_ARG(Unknown2)
		LOG_FUNC_END;

	// NOTE: This function is not meant to me emulated.  Just use it to find out
	// the function that is calling it, and emulate that instead!!!  If necessary,
	// create an XRef...

	//__asm int 3;
	EmuWarning("D3D::BlockOnTime not implemented (tell blueshogun)");

	LOG_UNIMPLEMENTED();
}

void WINAPI XTL::EMUPATCH(D3D_BlockOnResource)( X_D3DResource* pResource ) // TODO : Rename to BlockOnResource
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pResource);

	// TODO: Implement
	// NOTE: Azurik appears to call this directly from numerous points
	LOG_UNIMPLEMENTED();
		
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetPushBufferOffset)
(
	DWORD *pOffset
)
{
//	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(pOffset);

	// TODO: Implement
	*pOffset = 0;

	LOG_UNIMPLEMENTED();
}
#endif

#if 0 // Patch disabled
HRESULT WINAPI XTL::EMUPATCH(D3DCubeTexture_GetCubeMapSurface)
(
	X_D3DCubeTexture*	pThis,
	D3DCUBEMAP_FACES	FaceType,
	UINT				Level,
	X_D3DSurface**		ppCubeMapSurface
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pThis)
		LOG_FUNC_ARG(FaceType)
		LOG_FUNC_ARG(Level)
		LOG_FUNC_ARG(ppCubeMapSurface)
		LOG_FUNC_END;

	HRESULT hRet;

	EmuVerifyResourceIsRegistered(pThis);

	// Create a new surface
	*ppCubeMapSurface = EmuNewD3DSurface();
	IDirect3DCubeTexture8 *pHostCubeTexture = GetHostCubeTexture(pThis);
	IDirect3DSurface8 *pNewHostSurface = nullptr;

	hRet = pHostCubeTexture->GetCubeMapSurface(FaceType, Level, &pNewHostSurface);
	DEBUG_D3DRESULT(hRet, "pHostCubeTexture->GetCubeMapSurface");

	if (SUCCEEDED(hRet))
		SetHostSurface(*ppCubeMapSurface, pNewHostSurface);		

	return hRet;
}
#endif

#if 0 // Patch disabled
XTL::X_D3DSurface* WINAPI XTL::EMUPATCH(D3DCubeTexture_GetCubeMapSurface2)
(
	X_D3DCubeTexture*	pThis,
	D3DCUBEMAP_FACES	FaceType,
	UINT				Level
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DCubeTexture_GetCubeMapSurface");

	X_D3DSurface* pCubeMapSurface = NULL;

	EMUPATCH(D3DCubeTexture_GetCubeMapSurface)(pThis, FaceType, Level, &pCubeMapSurface);

	RETURN(pCubeMapSurface);
}
#endif

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_GetPixelShader)
(
	DWORD  Name,
	DWORD* pHandle
)
{
//	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Name)
		LOG_FUNC_ARG(pHandle)
		LOG_FUNC_END;

	// TODO: This implementation is very wrong, but better than nothing.
	*pHandle = g_dwCurrentPixelShader;
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetPersistedSurface)
(
	OUT X_D3DSurface **ppSurface
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG_OUT(ppSurface);

	// Attempt to load the persisted surface from persisted_surface.bmp

	*ppSurface = EmuNewD3DSurface();
	IDirect3DSurface8 *pNewHostSurface = nullptr;

	HRESULT hr = g_pD3DDevice8->CreateImageSurface( 640, 480, D3DFMT_X8R8G8B8, &pNewHostSurface);
	DEBUG_D3DRESULT(hr, "g_pD3DDevice8->CreateImageSurface - Could not create temporary surface!");

	if( SUCCEEDED( hr ) )
	{
		//SetHostSurface(*ppSurface, pNewHostSurface);
		hr = D3DXLoadSurfaceFromFileA(pNewHostSurface, nullptr, nullptr, "persisted_surface.bmp",
			nullptr, D3DX_DEFAULT, 0, nullptr);
		DEBUG_D3DRESULT(hr, "D3DXLoadSurfaceFromFileA");

		if( SUCCEEDED( hr ) )
			DbgPrintf( "Successfully loaded persisted_surface.bmp\n" );
	}
}

XTL::X_D3DSurface* WINAPI XTL::EMUPATCH(D3DDevice_GetPersistedSurface2)()
{
	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_GetPersistedSurface");

	X_D3DSurface* pSurface = NULL;

	EMUPATCH(D3DDevice_GetPersistedSurface)(&pSurface);

	RETURN(pSurface);
}

#if 0 // Patch disabled
VOID WINAPI XTL::EMUPATCH(D3DDevice_SetRenderTargetFast)
(
    X_D3DSurface	*pRenderTarget,
    X_D3DSurface	*pNewZStencil,
    DWORD			Flags
)
{
//	FUNC_EXPORTS

	LOG_FORWARD("D3DDevice_SetRenderTarget");

	// Redirect to the standard version.
	
	EMUPATCH(D3DDevice_SetRenderTarget)(pRenderTarget, pNewZStencil);
}
#endif

VOID WINAPI XTL::EMUPATCH(D3DDevice_GetScissors)
(
	DWORD	*pCount, 
	BOOL	*pExclusive, 
	D3DRECT *pRects
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(pCount)
		LOG_FUNC_ARG(pExclusive)
		LOG_FUNC_ARG(pRects)
		LOG_FUNC_END;

    // TODO: Save a copy of each scissor rect in case this function is called
	// in conjunction with D3DDevice::SetScissors. So far, only Outrun2 uses
	// this function. For now, just return the values within the current
	// viewport.

	D3DVIEWPORT8 vp;

	HRESULT hRet = g_pD3DDevice8->GetViewport( &vp );
	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->GetViewport");

	pRects->x1 = pRects->y1 = 0;
	pRects->x2 = vp.Width;
	pRects->y2 = vp.Height;

	pExclusive[0] = FALSE;
}

void WINAPI XTL::EMUPATCH(D3D_LazySetPointParams) // TODO : Rename to LazySetPointParams
(
	void* Device
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(Device);

	LOG_UNIMPLEMENTED();
}

// ******************************************************************
// * update the current milliseconds per frame
// ******************************************************************
// TODO : Move to own file
static void UpdateCurrentMSpFAndFPS() {
	if (g_EmuShared) {
		static float currentFPSVal = 30;

		float currentMSpFVal = (float)(1000.0 / (currentFPSVal == 0 ? 0.001 : currentFPSVal));
		g_EmuShared->SetCurrentMSpF(&currentMSpFVal);

		currentFPSVal = (float)(g_Frames*0.5 + currentFPSVal*0.5);
		g_EmuShared->SetCurrentFPS(&currentFPSVal);
	}
}
