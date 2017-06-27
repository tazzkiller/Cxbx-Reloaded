// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   CxbxKrnl->EmuD3D8Types.h
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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *  CopyRight (c) 2016-2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef EMUD3D8TYPES_H
#define EMUD3D8TYPES_H

#pragma region HostD3D // Host Direct3D declarations (for compatibility)

// include direct3d 8x types header files
#define DIRECT3D_VERSION 0x0800
#include <d3dx8.h>

#ifndef DXBX_USE_D3D9

typedef DWORD D3DDECLUSAGE;

// Direct3D8 has no SamplerStateType, so map it to TextureStageStateType :
#define D3DSAMPLERSTATETYPE D3DTEXTURESTAGESTATETYPE

#define D3DSAMP_ADDRESSU D3DTSS_ADDRESSU
#define D3DSAMP_ADDRESSV D3DTSS_ADDRESSV
#define D3DSAMP_ADDRESSW D3DTSS_ADDRESSW
#define D3DSAMP_BORDERCOLOR D3DTSS_BORDERCOLOR
#define D3DSAMP_MAGFILTER D3DTSS_MAGFILTER
#define D3DSAMP_MINFILTER D3DTSS_MINFILTER
#define D3DSAMP_MIPFILTER D3DTSS_MIPFILTER
#define D3DSAMP_MIPMAPLODBIAS D3DTSS_MIPMAPLODBIAS
#define D3DSAMP_MAXMIPLEVEL D3DTSS_MAXMIPLEVEL
#define D3DSAMP_MAXANISOTROPY D3DTSS_MAXANISOTROPY

#endif // DXBX_USE_D3D9

#define D3DSAMP_UNSUPPORTED ((D3DSAMPLERSTATETYPE)0)
#define D3DDECLUSAGE_UNSUPPORTED ((D3DDECLUSAGE)-1)

#ifndef DXBX_USE_D3D9
const DWORD
  D3DVSDE_FOG = D3DDECLUSAGE_UNSUPPORTED; // Doesn't exist in D3D8
#endif // DXBX_USE_D3D9

// Dxbx note : Some Xbox types are identical to the Direct3D8 declarations, here these forwards :
// TODO: fill out these types, instead of aliassing towards Direct3D8

#define X_D3DADAPTER_IDENTIFIER D3DADAPTER_IDENTIFIER8 // Same as on Windows Direct3D
#define X_D3DCAPS D3DCAPS8 // Same as on Windows Direct3D
#define X_D3DLIGHT D3DLIGHT8 // Same as on Windows Direct3D
#define X_D3DMATERIAL D3DMATERIAL8 // Same as on Windows Direct3D
#define X_D3DXMATRIX D3DXMATRIX // Same as on Windows Direct3D
#define X_D3DPOOL D3DPOOL // Same as on Windows Direct3D
#define X_D3DRECTPATCH_INFO D3DRECTPATCH_INFO // Same as on Windows Direct3D
#define X_D3DTRIPATCH_INFO D3DTRIPATCH_INFO // Same as on Windows Direct3D

// End of Host Direct3D related declarations
#pragma endregion


#pragma region XboxD3D // Xbox Direct3D declarations

// Xbox D3D Multisample type
typedef enum _X_D3DMULTISAMPLE_TYPE {
	X_D3DMULTISAMPLE_NONE = 0x0011,
	X_D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_LINEAR = 0x1021,
	X_D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_QUINCUNX = 0x1121,
	X_D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_HORIZONTAL_LINEAR = 0x2021,
	X_D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_VERTICAL_LINEAR = 0x2012,
	X_D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_LINEAR = 0x1022,
	X_D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_GAUSSIAN = 0x1222,
	X_D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_LINEAR = 0x2022,
	X_D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN = 0x2222,
	X_D3DMULTISAMPLE_9_SAMPLES_MULTISAMPLE_GAUSSIAN = 0x1233,
	X_D3DMULTISAMPLE_9_SAMPLES_SUPERSAMPLE_GAUSSIAN = 0x2233,
}
X_D3DMULTISAMPLE_TYPE;

// Xbox D3D Format
typedef enum _X_D3DFORMAT
{
/*
	Xbox1 D3DFORMAT notes
	---------------------

	The Xbox1 D3DFORMAT type consists of 4 different format categories :
	1. Swizzled (improves data locality, incompatible with native Direct3D)
	2. Compressed (DXT compression, giving 4:1 reduction on 4x4 pixel blocks)
	3. Linear (compatible with native Direct3D)
	4. Depth (Fixed or Floating point, stored Linear or Swizzled)

	Requirements\Format      Swizzled  Compressed  Linear  Depth   Notes

	Power-of-two required ?  YES       YES         NO      NO
	Mipmap supported ?       YES       YES         NO      YES     Linear has MipmapLevels = 1
	CubeMaps supported ?     YES       YES         NO      NO      Cubemaps have 6 faces
	Supports volumes ?       YES       YES         NO      NO      Volumes have 3 dimensions, Textures have 2
	Can be a rendertarget ?  YES       YES         YES     LINEAR  Depth buffers can only be rendered to if stored Linear

	Implications :
	- CubeMaps must be square
	- Volumes cannot be cube mapped and vice versa

	Maximum dimensions :
	2D : 4096 x 4096 (12 mipmap levels)
	3D : 512 x 512 x 512 (9 mipmap levels)

*/

	// Xbox D3DFORMAT types :
	// See http://wiki.beyondunreal.com/Legacy:Texture_Format

	// Swizzled Formats

	X_D3DFMT_L8 = 0x00,
	X_D3DFMT_AL8 = 0x01,
	X_D3DFMT_A1R5G5B5 = 0x02,
	X_D3DFMT_X1R5G5B5 = 0x03,
	X_D3DFMT_A4R4G4B4 = 0x04,
	X_D3DFMT_R5G6B5 = 0x05,
	X_D3DFMT_A8R8G8B8 = 0x06,
	X_D3DFMT_X8R8G8B8 = 0x07,
	X_D3DFMT_X8L8V8U8 = 0x07, // Alias

	X_D3DFMT_P8 = 0x0b, // 8-bit Palletized

	X_D3DFMT_A8 = 0x19,
	X_D3DFMT_A8L8 = 0x1a,
	X_D3DFMT_R6G5B5 = 0x27,
	X_D3DFMT_L6V5U5 = 0x27, // Alias

	X_D3DFMT_G8B8 = 0x28,
	X_D3DFMT_V8U8 = 0x28, // Alias

	X_D3DFMT_R8B8 = 0x29,
	X_D3DFMT_D24S8 = 0x2a,
	X_D3DFMT_F24S8 = 0x2b,
	X_D3DFMT_D16 = 0x2c,
	X_D3DFMT_D16_LOCKABLE = 0x2c, // Alias

	X_D3DFMT_F16 = 0x2d,
	X_D3DFMT_L16 = 0x32,
	X_D3DFMT_V16U16 = 0x33,
	X_D3DFMT_R5G5B5A1 = 0x38,
	X_D3DFMT_R4G4B4A4 = 0x39,
	X_D3DFMT_A8B8G8R8 = 0x3A,
	X_D3DFMT_Q8W8V8U8 = 0x3A, // Alias

	X_D3DFMT_B8G8R8A8 = 0x3B,
	X_D3DFMT_R8G8B8A8 = 0x3C,

	// YUV Formats

	X_D3DFMT_YUY2 = 0x24,
	X_D3DFMT_UYVY = 0x25,

	// Compressed Formats

	X_D3DFMT_DXT1 = 0x0C, // opaque/one-bit alpha
	X_D3DFMT_DXT2 = 0x0E, // Alias for X_D3DFMT_DXT3
	X_D3DFMT_DXT3 = 0x0E, // linear alpha
	X_D3DFMT_DXT4 = 0x0F, // Alias for X_D3DFMT_DXT5
	X_D3DFMT_DXT5 = 0x0F, // interpolated alpha

	// Linear Formats

	X_D3DFMT_LIN_A1R5G5B5 = 0x10,
	X_D3DFMT_LIN_R5G6B5 = 0x11,
	X_D3DFMT_LIN_A8R8G8B8 = 0x12,
	X_D3DFMT_LIN_L8 = 0x13,
	X_D3DFMT_LIN_R8B8 = 0x16,
	X_D3DFMT_LIN_G8B8 = 0x17,
	X_D3DFMT_LIN_V8U8 = 0x17, // Alias

	X_D3DFMT_LIN_AL8 = 0x1b,
	X_D3DFMT_LIN_X1R5G5B5 = 0x1c,
	X_D3DFMT_LIN_A4R4G4B4 = 0x1d,
	X_D3DFMT_LIN_X8R8G8B8 = 0x1e,
	X_D3DFMT_LIN_X8L8V8U8 = 0x1e, // Alias

	X_D3DFMT_LIN_A8 = 0x1f,
	X_D3DFMT_LIN_A8L8 = 0x20,
	X_D3DFMT_LIN_D24S8 = 0x2E,
	X_D3DFMT_LIN_F24S8 = 0x2f,
	X_D3DFMT_LIN_D16 = 0x30,
	X_D3DFMT_LIN_F16 = 0x31,
	X_D3DFMT_LIN_L16 = 0x35,
	X_D3DFMT_LIN_V16U16 = 0x36,
	X_D3DFMT_LIN_R6G5B5 = 0x37,
	X_D3DFMT_LIN_L6V5U5 = 0x37, // Alias

	X_D3DFMT_LIN_R5G5B5A1 = 0x3D,
	X_D3DFMT_LIN_R4G4B4A4 = 0x3e,
	X_D3DFMT_LIN_A8B8G8R8 = 0x3f,
	X_D3DFMT_LIN_B8G8R8A8 = 0x40,
	X_D3DFMT_LIN_R8G8B8A8 = 0x41,

	X_D3DFMT_VERTEXDATA = 0x64,

	X_D3DFMT_INDEX16 = 101/*=D3DFMT_INDEX16*/, // Dxbx addition : Not an Xbox format, used internally

	X_D3DFMT_UNKNOWN = 0xFFFFFFFF - 3,  // Unique declaration to make overloads possible
}
X_D3DFORMAT, *PX_D3DFORMAT;

// Xbox D3D Shade Mode
typedef enum _X_D3DSHADEMODE {
  X_D3DSHADE_FLAT               = 0x1d00,
  X_D3DSHADE_GOURAUD            = 0x1d01,
  X_D3DSHADE_FORCE_DWORD        = 0x7fffffff
}
X_D3DSHADEMODE;

// Xbox D3D Fill Mode
typedef enum _X_D3DFILLMODE {
  X_D3DFILL_POINT              = 0x1b00,
  X_D3DFILL_WIREFRAME          = 0x1b01,
  X_D3DFILL_SOLID              = 0x1b02,
  X_D3DFILL_FORCE_DWORD        = 0x7fffffff
}
X_D3DFILLMODE;

// Xbox D3D Blend
typedef enum _X_D3DBLEND {
  X_D3DBLEND_ZERO               = 0,
  X_D3DBLEND_ONE                = 1,
  X_D3DBLEND_SRCCOLOR           = 0x300,
  X_D3DBLEND_INVSRCCOLOR        = 0x301,
  X_D3DBLEND_SRCALPHA           = 0x302,
  X_D3DBLEND_INVSRCALPHA        = 0x303,
  X_D3DBLEND_DESTALPHA          = 0x304,
  X_D3DBLEND_INVDESTALPHA       = 0x305,
  X_D3DBLEND_DESTCOLOR          = 0x306,
  X_D3DBLEND_INVDESTCOLOR       = 0x307,
  X_D3DBLEND_SRCALPHASAT        = 0x308,
  X_D3DBLEND_CONSTANTCOLOR      = 0x8001,
  X_D3DBLEND_INVCONSTANTCOLOR   = 0x8002,
  X_D3DBLEND_CONSTANTALPHA      = 0x8003,
  X_D3DBLEND_INVCONSTANTALPHA   = 0x8004,
  X_D3DBLEND_FORCE_DWORD        = 0x7fffffff
}
X_D3DBLEND;

// Xbox D3D Blend Operation
typedef enum _X_D3DBLENDOP {
  X_D3DBLENDOP_ADD              = 0x8006,
  X_D3DBLENDOP_SUBTRACT         = 0x800a,
  X_D3DBLENDOP_REVSUBTRACT      = 0x800b,
  X_D3DBLENDOP_MIN              = 0x8007,
  X_D3DBLENDOP_MAX              = 0x8008,
  X_D3DBLENDOP_ADDSIGNED        = 0xf006,       // Xbox ext.
  X_D3DBLENDOP_REVSUBTRACTSIGNED= 0xf005,       // Xbox ext.
  X_D3DBLENDOP_FORCE_DWORD      = 0x7fffffff
}
X_D3DBLENDOP;

// Xbox D3D Cull
typedef enum _X_D3DCULL {
	X_D3DCULL_NONE = 0,
	X_D3DCULL_CW = 0x900,
	X_D3DCULL_CCW = 0x901,
	X_D3DCULL_FORCE_DWORD = 0x7fffffff
}
X_D3DCULL;

// Xbox D3D Front Cull
typedef enum _X_D3DFRONT { // Xbox ext.
  X_D3DFRONT_CW                 = 0x900,
  X_D3DFRONT_CCW                = 0x901,
  X_D3DFRONT_FORCE_DWORD        = 0x7fffffff
}
X_D3DFRONT;

// Xbox D3D Compare Function
typedef enum _X_D3DCMPFUNC {
  X_D3DCMP_NEVER                = 0x200,
  X_D3DCMP_LESS                 = 0x201,
  X_D3DCMP_EQUAL                = 0x202,
  X_D3DCMP_LESSEQUAL            = 0x203,
  X_D3DCMP_GREATER              = 0x204,
  X_D3DCMP_NOTEQUAL             = 0x205,
  X_D3DCMP_GREATEREQUAL         = 0x206,
  X_D3DCMP_ALWAYS               = 0x207,
  X_D3DCMP_FORCE_DWORD          = 0x7fffffff
}
X_D3DCMPFUNC;

// Xbox D3D Stencil Operation
typedef enum _X_D3DSTENCILOP {
  X_D3DSTENCILOP_ZERO           = 0,
  X_D3DSTENCILOP_KEEP           = 0x1e00,
  X_D3DSTENCILOP_REPLACE        = 0x1e01,
  X_D3DSTENCILOP_INCRSAT        = 0x1e02,
  X_D3DSTENCILOP_DECRSAT        = 0x1e03,
  X_D3DSTENCILOP_INVERT         = 0x150a,
  X_D3DSTENCILOP_INCR           = 0x8507,
  X_D3DSTENCILOP_DECR           = 0x8508,
  X_D3DSTENCILOP_FORCE_DWORD    = 0x7fffffff
}
X_D3DSTENCILOP;

// Xbox D3D Swath Width
typedef enum _X_D3DSWATHWIDTH { // Xbox ext
  X_D3DSWATH_8                  = 0,
  X_D3DSWATH_16                 = 1,
  X_D3DSWATH_32                 = 2,
  X_D3DSWATH_64                 = 3,
  X_D3DSWATH_128                = 4,
  X_D3DSWATH_OFF                = 0xf,
  X_D3DSWATH_FORCE_DWORD        = 0x7fffffff
}
X_D3DSWATHWIDTH;

// Xbox D3D Fog Mode
typedef enum _X_D3DFOGMODE {
  X_D3DFOG_NONE                 = 0,
  X_D3DFOG_EXP                  = 1,
  X_D3DFOG_EXP2                 = 2,
  X_D3DFOG_LINEAR               = 3,
  X_D3DFOG_FORCE_DWORD          = 0x7fffffff
}
X_D3DFOGMODE;

// Xbox D3D Logic Operation
typedef enum _X_D3DLOGICOP { // Xbox ext.
  X_D3DLOGICOP_NONE             = 0,
  X_D3DLOGICOP_CLEAR            = 0x1500,
  X_D3DLOGICOP_AND              = 0x1501,
  X_D3DLOGICOP_AND_REVERSE      = 0x1502,
  X_D3DLOGICOP_COPY             = 0x1503,
  X_D3DLOGICOP_AND_INVERTED     = 0x1504,
  X_D3DLOGICOP_NOOP             = 0x1505,
  X_D3DLOGICOP_XOR              = 0x1506,
  X_D3DLOGICOP_OR               = 0x1507,
  X_D3DLOGICOP_NOR              = 0x1508,
  X_D3DLOGICOP_EQUIV            = 0x1509,
  X_D3DLOGICOP_INVERT           = 0x150a,
  X_D3DLOGICOP_OR_REVERSE       = 0x150b,
  X_D3DLOGICOP_COPY_INVERTED    = 0x150c,
  X_D3DLOGICOP_OR_INVERTED      = 0x150d,
  X_D3DLOGICOP_NAND             = 0x150e,
  X_D3DLOGICOP_SET              = 0x150f,
  X_D3DLOGICOP_FORCE_DWORD      = 0x7fffffff
}
X_D3DLOGICOP;

// Xbox D3D Material Color Source (Values for material source)
typedef enum _X_D3DMATERIALCOLORSOURCE {
  X_D3DMCS_MATERIAL         = 0,            // Color from material is used
  X_D3DMCS_COLOR1           = 1,            // Diffuse vertex color is used
  X_D3DMCS_COLOR2           = 2,            // Specular vertex color is used
  X_D3DMCS_FORCE_DWORD      = 0x7fffffff
}
X_D3DMATERIALCOLORSOURCE;

// Xbox D3D Depth Clip Control flags (D3DRS_DEPTHCLIPCONTROL renderstate, Xbox ext.)
const DWORD X_D3DDCC_CULLPRIMITIVE = 0x001;
const DWORD X_D3DDCC_CLAMP         = 0x010;
const DWORD X_D3DDCC_IGNORE_W_SIGN = 0x100;

// Xbox D3D Multisample Mode
typedef enum _X_D3DMULTISAMPLEMODE {
  X_D3DMULTISAMPLEMODE_1X          = 0,
  X_D3DMULTISAMPLEMODE_2X          = 1,
  X_D3DMULTISAMPLEMODE_4X          = 2,
  X_D3DMULTISAMPLEMODE_FORCE_DWORD = 0x7fffffff
}
X_D3DMULTISAMPLEMODE;

// Xbox D3D Sample Alpha flags
const DWORD X_D3DSAMPLEALPHA_TOCOVERAGE = 0x0010;
const DWORD X_D3DSAMPLEALPHA_TOONE      = 0x0100;

// Xbox D3D Primitive Type (Primitives supported by draw-primitive API)
typedef enum _X_D3DPRIMITIVETYPE
{
    X_D3DPT_NONE = 0, // Dxbx addition

    X_D3DPT_POINTLIST             = 1,
    X_D3DPT_LINELIST              = 2,
    X_D3DPT_LINELOOP              = 3,    // Xbox only
    X_D3DPT_LINESTRIP             = 4,
    X_D3DPT_TRIANGLELIST          = 5,
    X_D3DPT_TRIANGLESTRIP         = 6,
    X_D3DPT_TRIANGLEFAN           = 7,
    X_D3DPT_QUADLIST              = 8,    // Xbox only
    X_D3DPT_QUADSTRIP             = 9,    // Xbox only
    X_D3DPT_POLYGON               = 10,   // Xbox only

    X_D3DPT_INVALID               = 0x7fffffff, /* force 32-bit size enum */
}
X_D3DPRIMITIVETYPE;

// Xbox D3D Transform State Type
typedef enum _X_D3DTRANSFORMSTATETYPE {
  X_D3DTS_VIEW          = 0,
  X_D3DTS_PROJECTION    = 1,
  X_D3DTS_TEXTURE0      = 2,
  X_D3DTS_TEXTURE1      = 3,
  X_D3DTS_TEXTURE2      = 4,
  X_D3DTS_TEXTURE3      = 5,
  X_D3DTS_WORLD         = 6,
  X_D3DTS_WORLD1        = 7,
  X_D3DTS_WORLD2        = 8,
  X_D3DTS_WORLD3        = 9,

  X_D3DTS_MAX           = 10, // Unused on Xbox
  X_D3DTS_FORCE_DWORD   = 0x7fffffff
}
X_D3DTRANSFORMSTATETYPE;

// Xbox D3D Resource Type
typedef enum _X_D3DRESOURCETYPE
{
    X_D3DRTYPE_NONE               =  0,
    X_D3DRTYPE_SURFACE            =  1,
    X_D3DRTYPE_VOLUME             =  2,
    X_D3DRTYPE_TEXTURE            =  3,
    X_D3DRTYPE_VOLUMETEXTURE      =  4,
    X_D3DRTYPE_CUBETEXTURE        =  5,
    X_D3DRTYPE_VERTEXBUFFER       =  6,
    X_D3DRTYPE_INDEXBUFFER        =  7,
    X_D3DRTYPE_PUSHBUFFER         =  8,
    X_D3DRTYPE_PALETTE            =  9,
    X_D3DRTYPE_FIXUP              =  10,

    X_D3DRTYPE_FORCE_DWORD        = 0x7fffffff
}
X_D3DRESOURCETYPE;

// Xbox D3D Swap Effect
typedef enum _X_D3DSWAPEFFECT
{
	X_D3DSWAPEFFECT_DISCARD = 1,
	X_D3DSWAPEFFECT_FLIP = 2,
	X_D3DSWAPEFFECT_COPY = 3,
	X_D3DSWAPEFFECT_COPY_VSYNC = 4,

	X_D3DSWAPEFFECT_FORCE_DWORD = 0x7fffffff
} X_D3DSWAPEFFECT;

const DWORD X_D3DPRESENTFLAG_LOCKABLE_BACKBUFFER = 0x00000001;
const DWORD X_D3DPRESENTFLAG_INTERLACED          = 0x00000020;
const DWORD X_D3DPRESENTFLAG_FIELD               = 0x00000080;

// Xbox D3DUSAGE values (all but the Xbox extensions match the PC versions) :
#define X_D3DUSAGE_RENDERTARGET           0x00000001
#define X_D3DUSAGE_DEPTHSTENCIL           0x00000002
// for Vertex/Index buffers
#define X_D3DUSAGE_WRITEONLY              0x00000008
#define X_D3DUSAGE_POINTS                 0x00000040
#define X_D3DUSAGE_RTPATCHES              0x00000080
#define X_D3DUSAGE_DYNAMIC                0x00000200
// for CreateVertexShader
#define X_D3DUSAGE_PERSISTENTDIFFUSE      0x00000400L   // Xbox-only
#define X_D3DUSAGE_PERSISTENTSPECULAR     0x00000800L   // Xbox-only
#define X_D3DUSAGE_PERSISTENTBACKDIFFUSE  0x00001000L   // Xbox-only
#define X_D3DUSAGE_PERSISTENTBACKSPECULAR 0x00002000L   // Xbox-only
// for CreateTexture/CreateImageSurface
#define X_D3DUSAGE_BORDERSOURCE_COLOR     0x00000000L   // Xbox-only
#define X_D3DUSAGE_BORDERSOURCE_TEXTURE   0x00010000L   // Xbox-only


// Xbox D3D Vertex Blend Flags
typedef enum _X_D3DVERTEXBLENDFLAGS {
    X_D3DVBF_DISABLE           = 0,     // Disable vertex blending
    X_D3DVBF_1WEIGHTS          = 1,     // 2 matrix blending
    X_D3DVBF_2WEIGHTS2MATRICES = 2,     // Xbox ext. nsp.
    X_D3DVBF_2WEIGHTS          = 3,     // 3 matrix blending
    X_D3DVBF_3WEIGHTS3MATRICES = 4,     // Xbox ext. nsp.
    X_D3DVBF_3WEIGHTS          = 5,     // 4 matrix blending
    X_D3DVBF_4WEIGHTS4MATRICES = 6,     // Xbox ext. nsp.

    X_D3DVBF_MAX               = 7,
    X_D3DVBF_FORCE_DWORD       = 0x7fffffff
}
X_D3DVERTEXBLENDFLAGS;

// Xbox D3D Copy Rectangle Color Format
typedef enum _X_D3DCOPYRECTCOLORFORMAT {
    D3DCOPYRECT_COLOR_FORMAT_DEFAULT                 = 0,
    D3DCOPYRECT_COLOR_FORMAT_Y8                      = 1,
    D3DCOPYRECT_COLOR_FORMAT_X1R5G5B5_Z1R5G5B5       = 2,
    D3DCOPYRECT_COLOR_FORMAT_X1R5G5B5_O1R5G5B5       = 3,
    D3DCOPYRECT_COLOR_FORMAT_R5G6B5                  = 4,
    D3DCOPYRECT_COLOR_FORMAT_Y16                     = 5,
    D3DCOPYRECT_COLOR_FORMAT_X8R8G8B8_Z8R8G8B8       = 6,
    D3DCOPYRECT_COLOR_FORMAT_X8R8G8B8_O8R8G8B8       = 7,
    D3DCOPYRECT_COLOR_FORMAT_X1A7R8G8B8_Z1A7R8G8B8   = 8,
    D3DCOPYRECT_COLOR_FORMAT_X1A7R8G8B8_O1A7R8G8B8   = 9,
    D3DCOPYRECT_COLOR_FORMAT_A8R8G8B8                = 10,
    D3DCOPYRECT_COLOR_FORMAT_Y32                     = 11,
    D3DCOPYRECT_COLOR_FORMAT_FORCE_DWORD             = 0x7fffffff //* force 32-bit size enum */
}
X_D3DCOPYRECTCOLORFORMAT;

// Xbox D3D Copy Rectangle Operation
typedef enum _X_D3DCOPYRECTOPERATION
{
    D3DCOPYRECT_SRCCOPY_AND         = 0,
    D3DCOPYRECT_ROP_AND             = 1,
    D3DCOPYRECT_BLEND_AND           = 2,
    D3DCOPYRECT_SRCCOPY             = 3,
    D3DCOPYRECT_SRCCOPY_PREMULT     = 4,
    D3DCOPYRECT_BLEND_PREMULT       = 5,
    D3DCOPYRECT_FORCE_DWORD         = 0x7fffffff // force 32-bit size enum */
}
X_D3DCOPYRECTOPERATION;

// Xbox D3D Copy Rectangle State
typedef struct _X_D3DCOPYRECTSTATE {
	X_D3DCOPYRECTCOLORFORMAT ColorFormat;
	X_D3DCOPYRECTOPERATION Operation;

	BOOL ColorKeyEnable;
	DWORD ColorKeyValue;

    // D3DCOPYRECT_BLEND_AND alpha value
    // The VALUE_FRACTION bits (30:21) contain the 10 bit unsigned fraction of the alpha value.
    // The VALUE bits (31:31) contain the 1 bit signed integer of the alpha value.
	DWORD BlendAlpha;

    // D3DCOPYRECT_*_PREMULT alpha value
    // Contains an alpha value for all four channels.
	DWORD PremultAlpha;

    // Clipping Rect
	DWORD ClippingPoint;    // y_x S16_S16
	DWORD ClippingSize;     // height_width U16_U16

}
X_D3DCOPYRECTSTATE;

// Xbox D3D Copy Rectangle Operation State
typedef struct _X_D3DCOPYRECTROPSTATE {            // Xbox extension
    DWORD Rop;              // Ternary raster operation.
                            //   DSTINVERT:0x55, SRCCOPY:0xCC,
                            //   SRCPAINT:0xEE, SRCINVERT:0x66,
                            //   ...

	DWORD Shape;            // 0:8X_8Y, 1:64X_1Y, 2:1X_64Y
	DWORD PatternSelect;    // 1:monochrome, 2:color

	DWORD MonoColor0;       // Color to use when bit is "0"
	DWORD MonoColor1;       // Color to use when bit is "1"

	DWORD MonoPattern0;     // 8x8 = 64 bit pattern
	DWORD MonoPattern1;     //

	PDWORD ColorPattern;       // Color Pattern used if PatternSelect == color
                                // 32-bit: Array of 64 DWORDS
                                // 16-bit: Array of 32 DWORDS
}
X_D3DCOPYRECTROPSTATE;

// Xbox D3D Set Depth Clip Plane Flags
typedef enum _X_D3DSET_DEPTH_CLIP_PLANES_FLAGS
{
    X_D3DSDCP_SET_VERTEXPROGRAM_PLANES         = 1,
    X_D3DSDCP_SET_FIXEDFUNCTION_PLANES         = 2,
    X_D3DSDCP_USE_DEFAULT_VERTEXPROGRAM_PLANES = 3,
    X_D3DSDCP_USE_DEFAULT_FIXEDFUNCTION_PLANES = 4,
} 
X_D3DSET_DEPTH_CLIP_PLANES_FLAGS;

// Xbox D3D Display Mode
typedef struct _X_D3DDISPLAYMODE
{
    UINT        Width;
    UINT        Height;
    UINT        RefreshRate;
    DWORD       Flags;
    X_D3DFORMAT Format;
}
X_D3DDISPLAYMODE;

// Xbox D3D Vertex Buffer Description
typedef struct _X_D3DVERTEXBUFFER_DESC
{
	X_D3DFORMAT           Format;
	X_D3DRESOURCETYPE     Type;
}
X_D3DVERTEXBUFFER_DESC;

// Xbox D3D Index Buffer Description
typedef struct _X_D3DINDEXBUFFER_DESC
{
	X_D3DFORMAT           Format;
	X_D3DRESOURCETYPE     Type;
}
X_D3DINDEXBUFFER_DESC;

// Xbox D3D Surface Description
typedef struct _X_D3DSURFACE_DESC
{
    X_D3DFORMAT         Format;
    X_D3DRESOURCETYPE   Type;
    DWORD               Usage;
    UINT                Size;
    X_D3DMULTISAMPLE_TYPE MultiSampleType;
    UINT                Width;
    UINT                Height;
}
X_D3DSURFACE_DESC;

// Xbox D3D Volume Description
typedef struct _X_D3DVOLUME_DESC {
    X_D3DFORMAT Format;
    X_D3DRESOURCETYPE Type;
    DWORD Usage;
    UINT Size;
    UINT Width;
    UINT Height;
    UINT Depth;
}
X_D3DVOLUME_DESC;

// Xbox D3D Present Parameters
typedef struct _X_D3DPRESENT_PARAMETERS
{
    UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    X_D3DFORMAT         BackBufferFormat;
    UINT                BackBufferCount;
    X_D3DMULTISAMPLE_TYPE MultiSampleType;
    X_D3DSWAPEFFECT     SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    X_D3DFORMAT         AutoDepthStencilFormat;
    DWORD               Flags;
    UINT                FullScreen_RefreshRateInHz;
    UINT                FullScreen_PresentationInterval;
    // The Windows DirectX8 variant ends here
    // This check guarantees identical layout, compared to Direct3D8._D3DPRESENT_PARAMETERS_:
    // Assert(Integer(@(PX_D3DPRESENT_PARAMETERS(nil).BufferSurfaces[0])) = SizeOf(_D3DPRESENT_PARAMETERS_));
    void  *BufferSurfaces[3]; // IDirect3DSurface8
    void  *DepthStencilSurface; // IDirect3DSurface8
}
X_D3DPRESENT_PARAMETERS;

// Xbox D3D Gamma Ramp Values
typedef struct _X_D3DGAMMARAMP
{
    BYTE    red[256];
    BYTE    green[256];
    BYTE    blue[256];
}
X_D3DGAMMARAMP;

// Xbox D3D Vertex Shader
struct X_D3DVertexShader
{
    union
    {
        DWORD   UnknownA;
        DWORD   Handle;
    };

    DWORD UnknownB;
    DWORD Flags;
    DWORD UnknownC[0x59];
};

// Xbox D3D Vertex Shader flags
const DWORD D3DVS_XBOX_RESERVEDXYZRHWSLOTS = 12;
const DWORD D3DVS_XBOX_NR_ADDRESS_SLOTS = 136; // Each slot is 4 DWORD's in size (see VSH_ENTRY_Bits)

// Xbox D3D Pixel Shader Definition
typedef struct _X_D3DPIXELSHADERDEF
{
	// X_D3DRS_PSALPHAINPUTS0 ... X_D3DRS_PSALPHAINPUTS7
	DWORD    PSAlphaInputs[8];          // Alpha inputs for each stage
	// DWORD X_D3DRS_PSFINALCOMBINERINPUTSABCD
	DWORD    PSFinalCombinerInputsABCD; // Final combiner inputs
	// X_D3DRS_PSFINALCOMBINERINPUTSEFG
	DWORD    PSFinalCombinerInputsEFG;  // Final combiner inputs (continued)
	// X_D3DRS_PSCONSTANT0_0 ... X_D3DRS_PSCONSTANT0_7
	DWORD    PSConstant0[8];            // C0 for each stage
	// X_D3DRS_PSCONSTANT1_0 ... X_D3DRS_PSCONSTANT1_7
	DWORD    PSConstant1[8];            // C1 for each stage
	// X_D3DRS_PSALPHAOUTPUTS0 ... X_D3DRS_PSALPHAOUTPUTS7
	DWORD    PSAlphaOutputs[8];         // Alpha output for each stage
	// X_D3DRS_PSRGBINPUTS0 ... X_D3DRS_PSRGBINPUTS7
	DWORD    PSRGBInputs[8];            // RGB inputs for each stage
	// X_D3DRS_PSCOMPAREMODE
	DWORD    PSCompareMode;             // Compare modes for clipplane texture mode
	// X_D3DRS_PSFINALCOMBINERCONSTANT0
	DWORD    PSFinalCombinerConstant0;  // C0 in final combiner
	// X_D3DRS_PSFINALCOMBINERCONSTANT1
	DWORD    PSFinalCombinerConstant1;  // C1 in final combiner
	// X_D3DRS_PSRGBOUTPUTS0 ... X_D3DRS_PSRGBOUTPUTS7
	DWORD    PSRGBOutputs[8];           // Stage 0 RGB outputs
	// X_D3DRS_PSCOMBINERCOUNT
	DWORD    PSCombinerCount;           // Active combiner count (Stages 0-7)
	// X_D3DRS_PS_RESERVED (X_D3DRS_PSTEXTUREMODES?)
	DWORD    PSTextureModes;            // Texture addressing modes
	// X_D3DRS_PSDOTMAPPING
	DWORD    PSDotMapping;              // Input mapping for dot product modes
	// X_D3DRS_PSINPUTTEXTURE
	DWORD    PSInputTexture;            // Texture source for some texture modes
	// Note : The above maps to X_D3DRS_PS_FIRST (X_D3DRS_PSALPHAINPUTS0) until X_D3DRS_PS_LAST (X_D3DRS_PSINPUTTEXTURE)

    // These last three DWORDs are used to define how Direct3D8 pixel shader constants map to the constant
    // registers in each combiner stage. They are used by the Direct3D run-time software but not by the hardware.
	DWORD    PSC0Mapping;               // Mapping of c0 regs to D3D constants
	DWORD    PSC1Mapping;               // Mapping of c1 regs to D3D constants
	DWORD    PSFinalCombinerConstants;  // Final combiner constant mapping
}
X_D3DPIXELSHADERDEF;

// Xbox D3D Resource Common flags and masks
#define X_D3DCOMMON_REFCOUNT_MASK      0x0000FFFF
#define X_D3DCOMMON_TYPE_MASK          0x00070000
#define X_D3DCOMMON_TYPE_SHIFT         16
#define X_D3DCOMMON_TYPE_VERTEXBUFFER  0x00000000
#define X_D3DCOMMON_TYPE_INDEXBUFFER   0x00010000
#define X_D3DCOMMON_TYPE_PUSHBUFFER    0x00020000
#define X_D3DCOMMON_TYPE_PALETTE       0x00030000
#define X_D3DCOMMON_TYPE_TEXTURE       0x00040000
#define X_D3DCOMMON_TYPE_SURFACE       0x00050000
#define X_D3DCOMMON_TYPE_FIXUP         0x00060000
#define X_D3DCOMMON_INTREFCOUNT_MASK   0x00780000
#define X_D3DCOMMON_INTREFCOUNT_SHIFT  19
const DWORD X_D3DCOMMON_INTREFCOUNT_1 = (1 << X_D3DCOMMON_INTREFCOUNT_SHIFT); // Dxbx addition
const DWORD X_D3DCOMMON_VIDEOMEMORY  = 0x00800000; // Not used.
#define X_D3DCOMMON_D3DCREATED         0x01000000
#define X_D3DCOMMON_ISLOCKED           0x02000010 // Surface is currently locked (potential unswizzle candidate)
#define X_D3DCOMMON_UNUSED_MASK        0xFE000000 // Dxbx has 0xFC000000
#define X_D3DCOMMON_UNUSED_SHIFT       25

// Xbox D3D Resource Lock flags
#define X_D3DLOCK_NOFLUSH               0x00000010 // Xbox extension
#define X_D3DLOCK_NOOVERWRITE           0x00000020
#define X_D3DLOCK_TILED                 0x00000040 // Xbox extension
#define X_D3DLOCK_READONLY              0x00000080

// Xbox D3D Resource
struct X_D3DResource
{
	DWORD Common;
	DWORD Data;
	DWORD Lock;
};

struct X_D3DVertexBuffer : public X_D3DResource
{
};

struct X_D3DIndexBuffer : public X_D3DResource
{
};

struct X_D3DPushBuffer : public X_D3DResource
{
    ULONG Size;
    ULONG AllocationSize;
};

struct X_D3DFixup : public X_D3DResource
{
    ULONG Run;
    ULONG Next;
    ULONG Size;
};

// Xbox D3D Palette Size
typedef enum _X_D3DPALETTESIZE
{
	D3DPALETTE_256 = 0,
	D3DPALETTE_128 = 1,
	D3DPALETTE_64 = 2,
	D3DPALETTE_32 = 3,
	D3DPALETTE_MAX = 4,
	D3DPALETTE_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
}
X_D3DPALETTESIZE;

// Xbox D3D Resource Common Palette Size masks
#define X_D3DPALETTE_COMMON_PALETTESIZE_MASK       0xC0000000
#define X_D3DPALETTE_COMMON_PALETTESIZE_SHIFT      30

struct X_D3DPalette : public X_D3DResource
{
};

// Xbox D3D Pixel Container Format flags and masks
#define X_D3DFORMAT_RESERVED1_MASK        0x00000003      // Must be zero
#define X_D3DFORMAT_DMACHANNEL_MASK       0x00000003
#define X_D3DFORMAT_DMACHANNEL_A          0x00000001      // DMA channel A - the default for all system memory
#define X_D3DFORMAT_DMACHANNEL_B          0x00000002      // DMA channel B - unused
#define X_D3DFORMAT_CUBEMAP               0x00000004      // Set if the texture if a cube map
#define X_D3DFORMAT_BORDERSOURCE_COLOR    0x00000008      // If set, uses D3DTSS_BORDERCOLOR as a border
#define X_D3DFORMAT_DIMENSION_MASK        0x000000F0      // # of dimensions
#define X_D3DFORMAT_DIMENSION_SHIFT       4
#define X_D3DFORMAT_FORMAT_MASK           0x0000FF00      // D3DFORMAT - See X_D3DFMT_* above
#define X_D3DFORMAT_FORMAT_SHIFT          8
#define X_D3DFORMAT_MIPMAP_MASK           0x000F0000      // # mipmap levels (always 1 for surfaces)
#define X_D3DFORMAT_MIPMAP_SHIFT          16
#define X_D3DFORMAT_USIZE_MASK            0x00F00000      // Log 2 of the U size of the base texture
#define X_D3DFORMAT_USIZE_SHIFT           20
#define X_D3DFORMAT_VSIZE_MASK            0x0F000000      // Log 2 of the V size of the base texture
#define X_D3DFORMAT_VSIZE_SHIFT           24
#define X_D3DFORMAT_PSIZE_MASK            0xF0000000      // Log 2 of the P size of the base texture
#define X_D3DFORMAT_PSIZE_SHIFT           28

// Xbox D3D Pixel Container Size Masks
// The layout of the size field, used for non swizzled or compressed textures.
//
// The Size field of a container will be zero if the texture is swizzled or compressed.
// It is guarenteed to be non-zero otherwise because either the height/width will be
// greater than one or the pitch adjust will be nonzero because the minimum texture
// pitch is 8 bytes.
#define X_D3DSIZE_WIDTH_MASK              0x00000FFF   // Width  (Texels - 1)
//#define X_D3DSIZE_WIDTH_SHIFT             0
#define X_D3DSIZE_HEIGHT_MASK             0x00FFF000   // Height (Texels - 1)
#define X_D3DSIZE_HEIGHT_SHIFT            12
#define X_D3DSIZE_PITCH_MASK              0xFF000000   // Pitch / 64 - 1
#define X_D3DSIZE_PITCH_SHIFT             24

// Xbox D3D Pixel Container
struct X_D3DPixelContainer : public X_D3DResource
{
	DWORD		Format; // Format information about the texture.
	DWORD       Size; // Size of a non power-of-2 texture, must be zero otherwise
};

#define X_D3D_RENDER_MEMORY_ALIGNMENT     64

#define X_D3DSURFACE_ALIGNMENT            X_D3D_RENDER_MEMORY_ALIGNMENT
#define X_D3DPALETTE_ALIGNMENT            X_D3D_RENDER_MEMORY_ALIGNMENT
#define X_D3DTEXTURE_ALIGNMENT            (2 * X_D3D_RENDER_MEMORY_ALIGNMENT)
#define X_D3DTEXTURE_CUBEFACE_ALIGNMENT   (2 * X_D3D_RENDER_MEMORY_ALIGNMENT)
#define X_D3DTEXTURE_PITCH_ALIGNMENT      X_D3D_RENDER_MEMORY_ALIGNMENT
#define X_D3DTEXTURE_PITCH_MIN            X_D3DTEXTURE_PITCH_ALIGNMENT

struct X_D3DBaseTexture : public X_D3DPixelContainer
{

};

struct X_D3DTexture : public X_D3DBaseTexture
{

};

struct X_D3DVolume : public X_D3DBaseTexture // Dxbx addition
{

};

struct X_D3DVolumeTexture : public X_D3DBaseTexture
{

};

struct X_D3DCubeTexture : public X_D3DBaseTexture
{

};

struct X_D3DSurface : public X_D3DPixelContainer
{
	X_D3DBaseTexture *Parent;
};

// Xbox D3D Tile
struct X_D3DTILE
{
    DWORD   Flags;
    PVOID   pMemory;
    DWORD   Size;
    DWORD   Pitch;
    DWORD   ZStartTag;
    DWORD   ZOffset;
};

// Xbox D3D Callback Type
typedef enum _X_D3DCALLBACKTYPE	// blueshogun96 10/1/07
{
	X_D3DCALLBACK_READ		= 0, // Fixed PatrickvL 10/7/22
	X_D3DCALLBACK_WRITE		= 1
}
X_D3DCALLBACKTYPE;

// Xbox D3D Field Type
typedef enum _X_D3DFIELDTYPE
{
    X_D3DFIELD_ODD            = 1,
    X_D3DFIELD_EVEN           = 2,
    X_D3DFIELD_PROGRESSIVE    = 3,
    X_D3DFIELD_FORCE_DWORD    = 0x7fffffff
}
X_D3DFIELDTYPE;

// Xbox D3D Field Status
typedef struct _X_D3DFIELD_STATUS
{
    X_D3DFIELDTYPE Field;
    UINT           VBlankCount;
}
X_D3DFIELD_STATUS;

// Xbox D3D Vertical Blank flags (Xbox ext.)
const DWORD D3DVBLANK_SWAPDONE   = 1;
const DWORD D3DVBLANK_SWAPMISSED = 2;

// Xbox D3D Vertical Blank Data
typedef struct _X_D3DVBLANKDATA
{
    DWORD           VBlankCounter;
    DWORD           SwapCounter;
    DWORD           Flags;
}
X_D3DVBLANKDATA;

// Xbox D3D Swap flags
const DWORD X_D3DSWAP_DEFAULT    = 0x00000000;
const DWORD X_D3DSWAP_COPY       = 0x00000001;
const DWORD X_D3DSWAP_BYPASSCOPY = 0x00000002;
const DWORD X_D3DSWAP_FINISH     = 0x00000004;

// Xbox D3D Swap Data
typedef struct _X_D3DSWAPDATA
{
    DWORD           Swap;
    DWORD           SwapVBlank;
    DWORD           MissedVBlanks;
    DWORD           TimeUntilSwapVBlank;
    DWORD           TimeBetweenSwapVBlanks;
} 
X_D3DSWAPDATA;

typedef void (__cdecl * X_D3DVBLANKCALLBACK)(X_D3DVBLANKDATA *pData);

typedef void (__cdecl * X_D3DSWAPCALLBACK)(X_D3DSWAPDATA *pData);

typedef void (__cdecl * X_D3DCALLBACK)(DWORD Context);


// Xbox D3D Render State Type
typedef DWORD X_D3DRENDERSTATETYPE;

// Xbox D3D Render States (X_D3DRENDERSTATETYPE values)

// Dxbx note : These declarations are from XDK version 5933, the most recent and complete version.
// Older versions are slightly different (some members are missing), so we use a mapping table to
// cater for the differences (see DxbxBuildRenderStateMappingTable). This enables to ignore these
// version-differences in the rest of our code (unless it matters somehow); We write via indirection :
//   *XTL::EmuMappedD3DRenderState[X_D3DRENDERSTATETYPE] = Value;
//
// And we read via the same mapping (do note, that missing elements all point to the same dummy) :
//   Result = *XTL::EmuMappedD3DRenderState[X_D3DRENDERSTATETYPE];

// Dxbx note : The X_D3DRS_PS* render states map 1-on-1 to the X_D3DPIXELSHADERDEF record,
// SetPixelShader actually pushes the definition into these render state slots.
// See DxbxUpdateActivePixelShader for how this is employed.

// The set starts out with "pixel-shader" render states (all Xbox extensions) :
const DWORD X_D3DRS_PSALPHAINPUTS0              = 0;
const DWORD X_D3DRS_PSALPHAINPUTS1              = 1;
const DWORD X_D3DRS_PSALPHAINPUTS2              = 2;
const DWORD X_D3DRS_PSALPHAINPUTS3              = 3;
const DWORD X_D3DRS_PSALPHAINPUTS4              = 4;
const DWORD X_D3DRS_PSALPHAINPUTS5              = 5;
const DWORD X_D3DRS_PSALPHAINPUTS6              = 6;
const DWORD X_D3DRS_PSALPHAINPUTS7              = 7;
const DWORD X_D3DRS_PSFINALCOMBINERINPUTSABCD   = 8;
const DWORD X_D3DRS_PSFINALCOMBINERINPUTSEFG    = 9;
const DWORD X_D3DRS_PSCONSTANT0_0               = 10;
const DWORD X_D3DRS_PSCONSTANT0_1               = 11;
const DWORD X_D3DRS_PSCONSTANT0_2               = 12;
const DWORD X_D3DRS_PSCONSTANT0_3               = 13;
const DWORD X_D3DRS_PSCONSTANT0_4               = 14;
const DWORD X_D3DRS_PSCONSTANT0_5               = 15;
const DWORD X_D3DRS_PSCONSTANT0_6               = 16;
const DWORD X_D3DRS_PSCONSTANT0_7               = 17;
const DWORD X_D3DRS_PSCONSTANT1_0               = 18;
const DWORD X_D3DRS_PSCONSTANT1_1               = 19;
const DWORD X_D3DRS_PSCONSTANT1_2               = 20;
const DWORD X_D3DRS_PSCONSTANT1_3               = 21;
const DWORD X_D3DRS_PSCONSTANT1_4               = 22;
const DWORD X_D3DRS_PSCONSTANT1_5               = 23;
const DWORD X_D3DRS_PSCONSTANT1_6               = 24;
const DWORD X_D3DRS_PSCONSTANT1_7               = 25;
const DWORD X_D3DRS_PSALPHAOUTPUTS0             = 26;
const DWORD X_D3DRS_PSALPHAOUTPUTS1             = 27;
const DWORD X_D3DRS_PSALPHAOUTPUTS2             = 28;
const DWORD X_D3DRS_PSALPHAOUTPUTS3             = 29;
const DWORD X_D3DRS_PSALPHAOUTPUTS4             = 30;
const DWORD X_D3DRS_PSALPHAOUTPUTS5             = 31;
const DWORD X_D3DRS_PSALPHAOUTPUTS6             = 32;
const DWORD X_D3DRS_PSALPHAOUTPUTS7             = 33;
const DWORD X_D3DRS_PSRGBINPUTS0                = 34;
const DWORD X_D3DRS_PSRGBINPUTS1                = 35;
const DWORD X_D3DRS_PSRGBINPUTS2                = 36;
const DWORD X_D3DRS_PSRGBINPUTS3                = 37;
const DWORD X_D3DRS_PSRGBINPUTS4                = 38;
const DWORD X_D3DRS_PSRGBINPUTS5                = 39;
const DWORD X_D3DRS_PSRGBINPUTS6                = 40;
const DWORD X_D3DRS_PSRGBINPUTS7                = 41;
const DWORD X_D3DRS_PSCOMPAREMODE               = 42;
const DWORD X_D3DRS_PSFINALCOMBINERCONSTANT0    = 43;
const DWORD X_D3DRS_PSFINALCOMBINERCONSTANT1    = 44;
const DWORD X_D3DRS_PSRGBOUTPUTS0               = 45;
const DWORD X_D3DRS_PSRGBOUTPUTS1               = 46;
const DWORD X_D3DRS_PSRGBOUTPUTS2               = 47;
const DWORD X_D3DRS_PSRGBOUTPUTS3               = 48;
const DWORD X_D3DRS_PSRGBOUTPUTS4               = 49;
const DWORD X_D3DRS_PSRGBOUTPUTS5               = 50;
const DWORD X_D3DRS_PSRGBOUTPUTS6               = 51;
const DWORD X_D3DRS_PSRGBOUTPUTS7               = 52;
const DWORD X_D3DRS_PSCOMBINERCOUNT             = 53;
const DWORD X_D3DRS_PS_RESERVED                 = 54; // Dxbx note : This takes the slot of X_D3DPIXELSHADERDEF.PSTextureModes, set by D3DDevice_SetRenderState_LogicOp?
const DWORD X_D3DRS_PSDOTMAPPING                = 55;
const DWORD X_D3DRS_PSINPUTTEXTURE              = 56;
// End of "pixel-shader" render states, continuing with "simple" render states :
const DWORD X_D3DRS_ZFUNC                       = 57; // D3DCMPFUNC
const DWORD X_D3DRS_ALPHAFUNC                   = 58; // D3DCMPFUNC
const DWORD X_D3DRS_ALPHABLENDENABLE            = 59; // TRUE to enable alpha blending
const DWORD X_D3DRS_ALPHATESTENABLE             = 60; // TRUE to enable alpha tests
const DWORD X_D3DRS_ALPHAREF                    = 61; // BYTE
const DWORD X_D3DRS_SRCBLEND                    = 62; // D3DBLEND
const DWORD X_D3DRS_DESTBLEND                   = 63; // D3DBLEND
const DWORD X_D3DRS_ZWRITEENABLE                = 64; // TRUE to enable Z writes
const DWORD X_D3DRS_DITHERENABLE                = 65; // TRUE to enable dithering
const DWORD X_D3DRS_SHADEMODE                   = 66; // D3DSHADEMODE
const DWORD X_D3DRS_COLORWRITEENABLE            = 67; // D3DCOLORWRITEENABLE_ALPHA, etc. per-channel write enable
const DWORD X_D3DRS_STENCILZFAIL                = 68; // D3DSTENCILOP to do if stencil test passes and Z test fails
const DWORD X_D3DRS_STENCILPASS                 = 69; // D3DSTENCILOP to do if both stencil and Z tests pass
const DWORD X_D3DRS_STENCILFUNC                 = 70; // D3DCMPFUNC
const DWORD X_D3DRS_STENCILREF                  = 71; // BYTE reference value used in stencil test
const DWORD X_D3DRS_STENCILMASK                 = 72; // BYTE mask value used in stencil test
const DWORD X_D3DRS_STENCILWRITEMASK            = 73; // BYTE write mask applied to values written to stencil buffer
const DWORD X_D3DRS_BLENDOP                     = 74; // D3DBLENDOP setting
const DWORD X_D3DRS_BLENDCOLOR                  = 75; // D3DCOLOR for D3DBLEND_CONSTANTCOLOR (Xbox ext.)
const DWORD X_D3DRS_SWATHWIDTH                  = 76; // D3DSWATHWIDTH (Xbox ext.)
const DWORD X_D3DRS_POLYGONOFFSETZSLOPESCALE    = 77; // float Z factor for shadow maps (Xbox ext.)
const DWORD X_D3DRS_POLYGONOFFSETZOFFSET        = 78; // Xbox ext.
const DWORD X_D3DRS_POINTOFFSETENABLE           = 79; // Xbox ext.
const DWORD X_D3DRS_WIREFRAMEOFFSETENABLE       = 80; // Xbox ext.
const DWORD X_D3DRS_SOLIDOFFSETENABLE           = 81; // Xbox ext.
const DWORD X_D3DRS_DEPTHCLIPCONTROL            = 82; // [4627+] Xbox ext.
const DWORD X_D3DRS_STIPPLEENABLE               = 83; // [4627+] Xbox ext.
const DWORD X_D3DRS_SIMPLE_UNUSED8              = 84; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED7              = 85; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED6              = 86; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED5              = 87; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED4              = 88; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED3              = 89; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED2              = 90; // [4627+]
const DWORD X_D3DRS_SIMPLE_UNUSED1              = 91; // [4627+]
// End of "simple" render states, continuing with "deferred" render states :
const DWORD X_D3DRS_FOGENABLE                   = 92;
const DWORD X_D3DRS_FOGTABLEMODE                = 93;
const DWORD X_D3DRS_FOGSTART                    = 94;
const DWORD X_D3DRS_FOGEND                      = 95;
const DWORD X_D3DRS_FOGDENSITY                  = 96;
const DWORD X_D3DRS_RANGEFOGENABLE              = 97;
const DWORD X_D3DRS_WRAP0                       = 98;
const DWORD X_D3DRS_WRAP1                       = 99;
const DWORD X_D3DRS_WRAP2                       = 100; // Dxbx addition
const DWORD X_D3DRS_WRAP3                       = 101; // Dxbx addition
const DWORD X_D3DRS_LIGHTING                    = 102;
const DWORD X_D3DRS_SPECULARENABLE              = 103;
const DWORD X_D3DRS_LOCALVIEWER                 = 104; // Dxbx addition
const DWORD X_D3DRS_COLORVERTEX                 = 105;
const DWORD X_D3DRS_BACKSPECULARMATERIALSOURCE  = 106; // Xbox ext. nsp.
const DWORD X_D3DRS_BACKDIFFUSEMATERIALSOURCE   = 107; // Xbox ext. nsp.
const DWORD X_D3DRS_BACKAMBIENTMATERIALSOURCE   = 108; // Xbox ext. nsp.
const DWORD X_D3DRS_BACKEMISSIVEMATERIALSOURCE  = 109; // Xbox ext. nsp.
const DWORD X_D3DRS_SPECULARMATERIALSOURCE      = 110;
const DWORD X_D3DRS_DIFFUSEMATERIALSOURCE       = 111;
const DWORD X_D3DRS_AMBIENTMATERIALSOURCE       = 112;
const DWORD X_D3DRS_EMISSIVEMATERIALSOURCE      = 113;
const DWORD X_D3DRS_BACKAMBIENT                 = 114; // Xbox ext. nsp.
const DWORD X_D3DRS_AMBIENT                     = 115;
const DWORD X_D3DRS_POINTSIZE                   = 116;
const DWORD X_D3DRS_POINTSIZE_MIN               = 117;
const DWORD X_D3DRS_POINTSPRITEENABLE           = 118;
const DWORD X_D3DRS_POINTSCALEENABLE            = 119;
const DWORD X_D3DRS_POINTSCALE_A                = 120;
const DWORD X_D3DRS_POINTSCALE_B                = 121;
const DWORD X_D3DRS_POINTSCALE_C                = 122;
const DWORD X_D3DRS_POINTSIZE_MAX               = 123;
const DWORD X_D3DRS_PATCHEDGESTYLE              = 124; // Dxbx addition
const DWORD X_D3DRS_PATCHSEGMENTS               = 125;
const DWORD X_D3DRS_SWAPFILTER                  = 126; // [4361+] Xbox ext. nsp. D3DTEXF_LINEAR etc. filter to use for Swap
const DWORD X_D3DRS_PRESENTATIONINTERVAL        = 127; // [4627+] Xbox ext. nsp.
const DWORD X_D3DRS_DEFERRED_UNUSED8            = 128; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED7            = 129; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED6            = 130; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED5            = 131; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED4            = 132; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED3            = 133; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED2            = 134; // [4627+]
const DWORD X_D3DRS_DEFERRED_UNUSED1            = 135; // [4627+]
// End of "deferred" render states, continuing with "complex" render states :
const DWORD X_D3DRS_PSTEXTUREMODES              = 136; // Xbox ext.
const DWORD X_D3DRS_VERTEXBLEND                 = 137;
const DWORD X_D3DRS_FOGCOLOR                    = 138;
const DWORD X_D3DRS_FILLMODE                    = 139;
const DWORD X_D3DRS_BACKFILLMODE                = 140; // Dxbx addition : Xbox ext. nsp.
const DWORD X_D3DRS_TWOSIDEDLIGHTING            = 141; // Dxbx addition : Xbox ext. nsp.
const DWORD X_D3DRS_NORMALIZENORMALS            = 142;
const DWORD X_D3DRS_ZENABLE                     = 143;
const DWORD X_D3DRS_STENCILENABLE               = 144;
const DWORD X_D3DRS_STENCILFAIL                 = 145;
const DWORD X_D3DRS_FRONTFACE                   = 146; // Dxbx addition : Xbox ext. nsp.
const DWORD X_D3DRS_CULLMODE                    = 147;
const DWORD X_D3DRS_TEXTUREFACTOR               = 148;
const DWORD X_D3DRS_ZBIAS                       = 149;
const DWORD X_D3DRS_LOGICOP                     = 150; // Xbox ext.
const DWORD X_D3DRS_EDGEANTIALIAS               = 151; // Dxbx note : No Xbox ext. (according to Direct3D8) !
const DWORD X_D3DRS_MULTISAMPLEANTIALIAS        = 152;
const DWORD X_D3DRS_MULTISAMPLEMASK             = 153;
const DWORD X_D3DRS_MULTISAMPLETYPE             = 154; // [-3911] Xbox ext. \_ aliasses  D3DMULTISAMPLE_TYPE
const DWORD X_D3DRS_MULTISAMPLEMODE             = 154; // [4361+] Xbox ext. /            D3DMULTISAMPLEMODE for the backbuffer
const DWORD X_D3DRS_MULTISAMPLERENDERTARGETMODE = 155; // [4361+] Xbox ext.
const DWORD X_D3DRS_SHADOWFUNC                  = 156; // D3DCMPFUNC (Xbox extension)
const DWORD X_D3DRS_LINEWIDTH                   = 157; // Xbox ext.
const DWORD X_D3DRS_SAMPLEALPHA                 = 158; // [4627+] Xbox ext. // TODO : Verify 4627 (might be introduced earlier)
const DWORD X_D3DRS_DXT1NOISEENABLE             = 159; // Xbox ext.
const DWORD X_D3DRS_YUVENABLE                   = 160; // [3911+] Xbox ext.
const DWORD X_D3DRS_OCCLUSIONCULLENABLE         = 161; // [3911+] Xbox ext.
const DWORD X_D3DRS_STENCILCULLENABLE           = 162; // [3911+] Xbox ext.
const DWORD X_D3DRS_ROPZCMPALWAYSREAD           = 163; // [3911+] Xbox ext.
const DWORD X_D3DRS_ROPZREAD                    = 164; // [3911+] Xbox ext.
const DWORD X_D3DRS_DONOTCULLUNCOMPRESSED       = 165; // [3911+] Xbox ext.
// End of "complex" render states.

// Xbox D3D Wrap Values
const DWORD X_D3DWRAP_U = 0x00000010;
const DWORD X_D3DWRAP_V = 0x00001000;
const DWORD X_D3DWRAP_W = 0x00100000;

// Xbox D3D Texture Stage State Type
typedef DWORD X_D3DTEXTURESTAGESTATETYPE;

// Xbox D3D Texture Stage State (X_D3DTEXTURESTAGESTATETYPE values)
// Dxbx note : See DxbxFromOldVersion_D3DTSS(), as these might need correction for older SDK versions!
// The set starts out with "deferred" texture states :
const DWORD X_D3DTSS_ADDRESSU = 0;
const DWORD X_D3DTSS_ADDRESSV = 1;
const DWORD X_D3DTSS_ADDRESSW = 2;
const DWORD X_D3DTSS_MAGFILTER = 3;
const DWORD X_D3DTSS_MINFILTER = 4;
const DWORD X_D3DTSS_MIPFILTER = 5;
const DWORD X_D3DTSS_MIPMAPLODBIAS = 6;
const DWORD X_D3DTSS_MAXMIPLEVEL = 7;
const DWORD X_D3DTSS_MAXANISOTROPY = 8;
const DWORD X_D3DTSS_COLORKEYOP = 9; // Xbox ext.
const DWORD X_D3DTSS_COLORSIGN = 10; // Xbox ext.
const DWORD X_D3DTSS_ALPHAKILL = 11; // Xbox ext.
const DWORD X_D3DTSS_COLOROP = 12;
const DWORD X_D3DTSS_COLORARG0 = 13;
const DWORD X_D3DTSS_COLORARG1 = 14;
const DWORD X_D3DTSS_COLORARG2 = 15;
const DWORD X_D3DTSS_ALPHAOP = 16;
const DWORD X_D3DTSS_ALPHAARG0 = 17;
const DWORD X_D3DTSS_ALPHAARG1 = 18;
const DWORD X_D3DTSS_ALPHAARG2 = 19;
const DWORD X_D3DTSS_RESULTARG = 20;
const DWORD X_D3DTSS_TEXTURETRANSFORMFLAGS = 21;
// End of "deferred" texture states, continuing with the rest :
const DWORD X_D3DTSS_BUMPENVMAT00 = 22;
const DWORD X_D3DTSS_BUMPENVMAT01 = 23;
const DWORD X_D3DTSS_BUMPENVMAT11 = 24;
const DWORD X_D3DTSS_BUMPENVMAT10 = 25;
const DWORD X_D3DTSS_BUMPENVLSCALE = 26;
const DWORD X_D3DTSS_BUMPENVLOFFSET = 27;
const DWORD X_D3DTSS_TEXCOORDINDEX = 28;
const DWORD X_D3DTSS_BORDERCOLOR = 29;
const DWORD X_D3DTSS_COLORKEYCOLOR = 30; // Xbox ext.
const DWORD X_D3DTSS_UNSUPPORTED = 31; // Note : Somehow, this one comes through D3DDevice_SetTextureStageStateNotInline sometimes
// End of texture states.

// Xbox D3D Texture Operation
typedef DWORD X_D3DTEXTUREOP;

// Xbox D3D Texture Operation (X_D3DTEXTUREOP values)
#define X_D3DTOP_DISABLE 1
#define X_D3DTOP_SELECTARG1 2
#define X_D3DTOP_SELECTARG2 3
#define X_D3DTOP_MODULATE  4
#define X_D3DTOP_MODULATE2X  5
#define X_D3DTOP_MODULATE4X  6
#define X_D3DTOP_ADD  7
#define X_D3DTOP_ADDSIGNED  8
#define X_D3DTOP_ADDSIGNED2X  9
#define X_D3DTOP_SUBTRACT  10
#define X_D3DTOP_ADDSMOOTH  11
#define X_D3DTOP_BLENDDIFFUSEALPHA  12
#define X_D3DTOP_BLENDCURRENTALPHA  13
#define X_D3DTOP_BLENDTEXTUREALPHA  14
#define X_D3DTOP_BLENDFACTORALPHA  15
#define X_D3DTOP_BLENDTEXTUREALPHAPM  16
#define X_D3DTOP_PREMODULATE  17
#define X_D3DTOP_MODULATEALPHA_ADDCOLOR  18
#define X_D3DTOP_MODULATECOLOR_ADDALPHA  19
#define X_D3DTOP_MODULATEINVALPHA_ADDCOLOR  20
#define X_D3DTOP_MODULATEINVCOLOR_ADDALPHA  21
#define X_D3DTOP_DOTPRODUCT3  22
#define X_D3DTOP_MULTIPLYADD  23
#define X_D3DTOP_LERP  24
#define X_D3DTOP_BUMPENVMAP  25
#define X_D3DTOP_BUMPENVMAPLUMINANCE  26

// Xbox D3D Texture Addressing mode (for X_D3DRS_PSTEXTUREMODES)
typedef DWORD X_D3DTEXTUREADDRESS;

// Xbox D3D Texture Addressing Mode (X_D3DTEXTUREADDRESS values)
const DWORD X_D3DTADDRESS_WRAP = 1;
const DWORD X_D3DTADDRESS_MIRROR = 2;
const DWORD X_D3DTADDRESS_CLAMP = 3;
const DWORD X_D3DTADDRESS_BORDER = 4;
const DWORD X_D3DTADDRESS_CLAMPTOEDGE = 5;

// Xbox D3D Texture Filter Type
typedef DWORD X_D3DTEXTUREFILTERTYPE;

// Xbox D3D Texture Filter (X_D3DTEXTUREFILTERTYPE values)
const DWORD X_D3DTEXF_NONE = 0;          // filtering disabled (valid for mip filter only)
const DWORD X_D3DTEXF_POINT = 1;         // nearest
const DWORD X_D3DTEXF_LINEAR = 2;        // linear interpolation
const DWORD X_D3DTEXF_ANISOTROPIC = 3;   // anisotropic
const DWORD X_D3DTEXF_QUINCUNX = 4;      // quincunx kernel (Xbox extension)
const DWORD X_D3DTEXF_GAUSSIANCUBIC = 5; // different cubic kernel

// Xbox D3D Clear
typedef DWORD X_D3DCLEAR;

// Xbox D3D Clear (X_D3DCLEAR flags and masks)
#define X_D3DCLEAR_ZBUFFER  0x00000001
#define X_D3DCLEAR_STENCIL  0x00000002
#define X_D3DCLEAR_TARGET_R 0x00000010 // Xbox ext.
#define X_D3DCLEAR_TARGET_G 0x00000020 // Xbox ext.
#define X_D3DCLEAR_TARGET_B 0x00000040 // Xbox ext.
#define X_D3DCLEAR_TARGET_A 0x00000080 // Xbox ext.
#define X_D3DCLEAR_TARGET   (X_D3DCLEAR_TARGET_R | X_D3DCLEAR_TARGET_G | X_D3DCLEAR_TARGET_B | X_D3DCLEAR_TARGET_A)

// Xbox D3D Color Write Enable (for X_D3DRS_COLORWRITEENABLE)
typedef DWORD X_D3DCOLORWRITEENABLE;

// Xbox D3D Color Write Enable flags and masks (X_D3DCOLORWRITEENABLE  values)
const DWORD X_D3DCOLORWRITEENABLE_RED = (1 << 16);
const DWORD X_D3DCOLORWRITEENABLE_GREEN = (1 << 8);
const DWORD X_D3DCOLORWRITEENABLE_BLUE = (1 << 0);
const DWORD X_D3DCOLORWRITEENABLE_ALPHA = (1 << 24);
const DWORD X_D3DCOLORWRITEENABLE_ALL = 0x01010101; // Xbox ext.

// Xbox D3D Texture Stage State "unknown" flag
const DWORD X_D3DTSS_UNKNOWN = 0x7fffffff;

// Xbox D3D Render State "unknown" flag
const DWORD X_D3DRS_UNKNOWN = 0x7fffffff;

// Xbox D3D Shader Constant Mode
typedef DWORD X_D3DSHADERCONSTANTMODE;

// Xbox D3D Shader Constant Mode flags (X_D3DSHADERCONSTANTMODE values)
const DWORD X_D3DSCM_96CONSTANTS                  = 0x00; // Enables constants 0..95
const DWORD X_D3DSCM_192CONSTANTS                 = 0x01; // Enables constants -96..-1 on top of 0..95
const DWORD X_D3DSCM_192CONSTANTSANDFIXEDPIPELINE = 0x02; // Unsupported?
const DWORD X_D3DSCM_NORESERVEDCONSTANTS          = 0x10;  // Do not reserve constant -38 and -37

const DWORD X_D3DSCM_RESERVED_CONSTANT1 = -38; // Becomes 58 after correction, contains Scale v
const DWORD X_D3DSCM_RESERVED_CONSTANT2 = -37; // Becomes 59 after correction, contains Offset

const DWORD X_D3DSCM_CORRECTION = 96; // Add 96 to arrive at the range 0..191 (instead of 96..95)

// Xbox D3D Vertex Shader Constant Register Count
const DWORD X_D3DVS_CONSTREG_COUNT = 192;

// Xbox D3D Vertex Shader Type flags
#define X_VST_NORMAL                  1
#define X_VST_READWRITE               2
#define X_VST_STATE                   3

// Xbox D3D Vertex Shader Input
typedef struct _X_VERTEXSHADERINPUT
{
    DWORD IndexOfStream;
    DWORD Offset;
    DWORD Format;
    BYTE  TesselationType;
    BYTE  TesselationSource;
}
X_VERTEXSHADERINPUT;

// Xbox D3D Vertex Attribute Format
typedef struct _X_VERTEXATTRIBUTEFORMAT
{
    X_VERTEXSHADERINPUT pVertexShaderInput[16];
}
X_VERTEXATTRIBUTEFORMAT;

// Xbox D3D Stream Input
typedef struct _X_STREAMINPUT
{
    X_D3DVertexBuffer  *VertexBuffer;
    UINT                Stride;
    UINT                Offset;
} X_STREAMINPUT;

// vertex shader input registers for fixed function vertex shader

//          Name                   Register number      D3DFVF
const int X_D3DVSDE_POSITION     = 0; // Corresponds to D3DFVF_XYZ
const int X_D3DVSDE_BLENDWEIGHT  = 1; // Corresponds to D3DFVF_XYZRHW
const int X_D3DVSDE_NORMAL       = 2; // Corresponds to D3DFVF_NORMAL
const int X_D3DVSDE_DIFFUSE      = 3; // Corresponds to D3DFVF_DIFFUSE
const int X_D3DVSDE_SPECULAR     = 4; // Corresponds to D3DFVF_SPECULAR
const int X_D3DVSDE_FOG          = 5; // Xbox extension
const int X_D3DVSDE_POINTSIZE    = 6; // Dxbx addition
const int X_D3DVSDE_BACKDIFFUSE  = 7; // Xbox extension
const int X_D3DVSDE_BACKSPECULAR = 8; // Xbox extension
const int X_D3DVSDE_TEXCOORD0    = 9; // "Corresponds to D3DFVF_TEX0" says the docs, but 0 means no textures, so probably D3DFVF_TEX1!
const int X_D3DVSDE_TEXCOORD1    = 10; // Corresponds to D3DFVF_TEX{above}+1
const int X_D3DVSDE_TEXCOORD2    = 11; // Corresponds to D3DFVF_TEX{above}+2
const int X_D3DVSDE_TEXCOORD3    = 12; // Corresponds to D3DFVF_TEX{above}+3
const int X_D3DVSDE_VERTEX       = 0xFFFFFFFF; // Xbox extension for Begin/End drawing (data is a D3DVSDT_FLOAT4)

//typedef X_D3DVSDE = X_D3DVSDE_POSITION..High(DWORD)-2; // Unique declaration to make overloads possible;

// bit declarations for _Type fields
const int X_D3DVSDT_FLOAT1      = 0x12; // 1D float expanded to (value, 0.0, 0.0, 1.0)
const int X_D3DVSDT_FLOAT2      = 0x22; // 2D float expanded to (value, value, 0.0, 1.0)
const int X_D3DVSDT_FLOAT3      = 0x32; // 3D float expanded to (value, value, value, 1.0) In double word format this is ARGB, or in byte ordering it would be B, G, R, A.
const int X_D3DVSDT_FLOAT4      = 0x42; // 4D float
const int X_D3DVSDT_D3DCOLOR    = 0x40; // 4D packed unsigned bytes mapped to 0.0 to 1.0 range
//const int X_D3DVSDT_UBYTE4      = 0x05; // 4D unsigned byte   Dxbx note : Not supported on Xbox ?
const int X_D3DVSDT_SHORT2      = 0x25; // 2D signed short expanded to (value, value, 0.0, 1.0)
const int X_D3DVSDT_SHORT4      = 0x45; // 4D signed short

//  Xbox only declarations :
const int X_D3DVSDT_NORMSHORT1  = 0x11; // xbox ext. 1D signed, normalized short expanded to (value, 0.0, 0.0, 1.0). Signed, normalized shorts map from -1.0 to 1.0.
const int X_D3DVSDT_NORMSHORT2  = 0x21; // xbox ext. 2D signed, normalized short expanded to (value, value, 0.0, 1.0). Signed, normalized shorts map from -1.0 to 1.0.
const int X_D3DVSDT_NORMSHORT3  = 0x31; // xbox ext. 3D signed, normalized short expanded to (value, value, value, 1.0). Signed, normalized shorts map from -1.0 to 1.0.
const int X_D3DVSDT_NORMSHORT4  = 0x41; // xbox ext. 4D signed, normalized short expanded to (value, value, value, value). Signed, normalized shorts map from -1.0 to 1.0.
const int X_D3DVSDT_NORMPACKED3 = 0x16; // xbox ext. Three signed, normalized components packed in 32-bits. (11,11,10). Each component ranges from -1.0 to 1.0. Expanded to (value, value, value, 1.0).
const int X_D3DVSDT_SHORT1      = 0x15; // xbox ext. 1D signed short expanded to (value, 0., 0., 1). Signed shorts map to the range [-32768, 32767].
const int X_D3DVSDT_SHORT3      = 0x35; // xbox ext. 3D signed short expanded to (value, value, value, 1). Signed shorts map to the range [-32768, 32767].
const int X_D3DVSDT_PBYTE1      = 0x14; // xbox ext. 1D packed byte expanded to (value, 0., 0., 1). Packed bytes map to the range [0, 1].
const int X_D3DVSDT_PBYTE2      = 0x24; // xbox ext. 2D packed byte expanded to (value, value, 0., 1). Packed bytes map to the range [0, 1].
const int X_D3DVSDT_PBYTE3      = 0x34; // xbox ext. 3D packed byte expanded to (value, value, value, 1). Packed bytes map to the range [0, 1].
const int X_D3DVSDT_PBYTE4      = 0x44; // xbox ext. 4D packed byte expanded to (value, value, value, value). Packed bytes map to the range [0, 1].
const int X_D3DVSDT_FLOAT2H     = 0x72; // xbox ext. 3D float that expands to (value, value, 0.0, value). Useful for projective texture coordinates.
const int X_D3DVSDT_NONE        = 0x02; // xbox ext. nsp

const int MAX_NBR_STREAMS = 16; // Dxbx addition?

typedef WORD INDEX16;

// unused #define X_D3DVSD_MASK_TESSUV 0x10000000
#define X_D3DVSD_MASK_SKIP 0x10000000 // Skips (normally) dwords
#define X_D3DVSD_MASK_SKIPBYTES 0x08000000 // Skips bytes (no, really?!)

#define X_D3DVSD_TOKENTYPESHIFT   29
#define X_D3DVSD_TOKENTYPEMASK    (7 << X_D3DVSD_TOKENTYPESHIFT)

#define X_D3DVSD_STREAMNUMBERSHIFT 0
#define X_D3DVSD_STREAMNUMBERMASK (0xF << X_D3DVSD_STREAMNUMBERSHIFT)

#define X_D3DVSD_DATALOADTYPESHIFT 28
#define X_D3DVSD_DATALOADTYPEMASK (0x1 << X_D3DVSD_DATALOADTYPESHIFT)

#define X_D3DVSD_DATATYPESHIFT 16
#define X_D3DVSD_DATATYPEMASK (0xFF << X_D3DVSD_DATATYPESHIFT)

#define X_D3DVSD_SKIPCOUNTSHIFT 16
#define X_D3DVSD_SKIPCOUNTMASK (0xF << X_D3DVSD_SKIPCOUNTSHIFT)

#define X_D3DVSD_VERTEXREGSHIFT 0
#define X_D3DVSD_VERTEXREGMASK (0x1F << X_D3DVSD_VERTEXREGSHIFT)

#define X_D3DVSD_VERTEXREGINSHIFT 20
#define X_D3DVSD_VERTEXREGINMASK (0xF << X_D3DVSD_VERTEXREGINSHIFT)

#define X_D3DVSD_CONSTCOUNTSHIFT 25
#define X_D3DVSD_CONSTCOUNTMASK (0xF << X_D3DVSD_CONSTCOUNTSHIFT)

#define X_D3DVSD_CONSTADDRESSSHIFT 0
#define X_D3DVSD_CONSTADDRESSMASK (0xFF << X_D3DVSD_CONSTADDRESSSHIFT)

#define X_D3DVSD_CONSTRSSHIFT 16
#define X_D3DVSD_CONSTRSMASK (0x1FFF << X_D3DVSD_CONSTRSSHIFT)

#define X_D3DVSD_EXTCOUNTSHIFT 24
#define X_D3DVSD_EXTCOUNTMASK (0x1F << X_D3DVSD_EXTCOUNTSHIFT)

#define X_D3DVSD_EXTINFOSHIFT 0
#define X_D3DVSD_EXTINFOMASK (0xFFFFFF << X_D3DVSD_EXTINFOSHIFT)

// End of Xbox Direct3D declarations
#pragma endregion


#pragma region CxbxD3D // Cxbx Direct3D declarations (not present in Xbox)

typedef DWORD NV2AMETHOD;

// Cxbx special resource data flags (must set _SPECIAL *AND* specific flag(s))
#define CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL 0xFFFF0000
#define CXBX_D3DRESOURCE_DATA_BACK_BUFFER (CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL | 0x00000001)
//#define CXBX_D3DRESOURCE_DATA_SURFACE_LEVEL (CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL | 0x00000002) // unused
#define CXBX_D3DRESOURCE_DATA_RENDER_TARGET (CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL | 0x00000004)
#define CXBX_D3DRESOURCE_DATA_DEPTH_STENCIL (CXBX_D3DRESOURCE_DATA_FLAG_SPECIAL | 0x00000008)

// Render state boundaries :
#define X_D3DRS_PS_FIRST X_D3DRS_PSALPHAINPUTS0
#define X_D3DRS_PS_LAST X_D3DRS_PSINPUTTEXTURE
#define X_D3DRS_SIMPLE_FIRST X_D3DRS_ZFUNC
#define X_D3DRS_SIMPLE_LAST X_D3DRS_SIMPLE_UNUSED1
#define X_D3DRS_DEFERRED_FIRST X_D3DRS_FOGENABLE
#define X_D3DRS_DEFERRED_LAST X_D3DRS_DEFERRED_UNUSED1
#define X_D3DRS_COMPLEX_FIRST X_D3DRS_PSTEXTUREMODES
#define X_D3DRS_COMPLEX_LAST X_D3DRS_DONOTCULLUNCOMPRESSED
#define X_D3DRS_FIRST X_D3DRS_PS_FIRST
#define X_D3DRS_LAST X_D3DRS_COMPLEX_LAST

// Texture state boundaries :
#define X_D3DTSS_DEFERRED_FIRST X_D3DTSS_ADDRESSU
#define X_D3DTSS_DEFERRED_LAST X_D3DTSS_TEXTURETRANSFORMFLAGS
#define X_D3DTSS_OTHER_FIRST X_D3DTSS_BUMPENVMAT00
#define X_D3DTSS_OTHER_LAST X_D3DTSS_COLORKEYCOLOR // Not X_D3DTSS_UNSUPPORTED until we know for sure
#define X_D3DTSS_FIRST X_D3DTSS_DEFERRED_FIRST
#define X_D3DTSS_LAST X_D3DTSS_OTHER_LAST
#define X_D3DTSS_STAGECOUNT 4
#define X_D3DTSS_STAGESIZE 32

#define X_PSH_COMBINECOUNT 8
#define X_PSH_CONSTANTCOUNT 8

#define X_PIXELSHADER_FAKE_HANDLE 0xDEADBEEF

typedef struct _CxbxPixelShader
{
	//IDirect3DPixelShader9* pShader;
	//ID3DXConstantTable *pConstantTable;
	DWORD Handle;

	BOOL bBumpEnvMap;

	// constants
	DWORD PSRealC0[8];
	DWORD PSRealC1[8];
	DWORD PSRealFC0;
	DWORD PSRealFC1;

	BOOL bConstantsInitialized;
	BOOL bConstantsChanged;

	DWORD dwStatus;
	X_D3DPIXELSHADERDEF	PSDef;

	DWORD dwStageMap[4]; // = X_D3DTSS_STAGECOUNT

}
CxbxPixelShader;

typedef struct _CxbxStreamDynamicPatch
{
	BOOL  NeedPatch;       // This is to know whether is data which must be patched
	DWORD ConvertedStride;
	DWORD NbrTypes;        // Number of the stream data types
	UINT  *pTypes;         // The stream data types (xbox)
	UINT  *pSizes;         // The stream data sizes (pc)
}
CxbxStreamDynamicPatch;

typedef struct _CxbxVertexDynamicPatch
{
	UINT                         NbrStreams; // The number of streams the vertex shader uses
	CxbxStreamDynamicPatch      *pStreamPatches;
}
CxbxVertexShaderDynamicPatch;

typedef struct _CxbxVertexShader
{
	DWORD Handle;

	// These are the parameters given by the XBE,
	// we save them to be be able to return them when necassary.
	UINT                  Size;
	DWORD                *pDeclaration;
	DWORD                 DeclarationSize;
	DWORD                *pFunction;
	DWORD                 FunctionSize;
	DWORD                 Type;
	DWORD                 Status;

	// Needed for dynamic stream patching
	CxbxVertexShaderDynamicPatch  VertexShaderDynamicPatch;
}
CxbxVertexShader;

// End of Cxbx Direct3D declarations
#pragma endregion

#endif
