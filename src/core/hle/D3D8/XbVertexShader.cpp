// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// ******************************************************************
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
// *  (c) 2002-2004 Aaron Robinson <caustik@caustik.com>
// *                Kingofc <kingofc@freenet.de>
// *
// *  All rights reserved
// *
// ******************************************************************
#define LOG_PREFIX CXBXR_MODULE::VTXSH

#define _DEBUG_TRACK_VS

#include "core\kernel\init\CxbxKrnl.h"
#include "core\kernel\support\Emu.h"
#include "core\kernel\support\EmuFS.h"
#include "core\kernel\support\EmuXTL.h"
#include "core\hle\Intercept.hpp" // For XB_trampoline
#include "XbD3D8Types.h" // For X_D3DVSDE_*
#include <sstream>
#include <unordered_map>
#include <array>

#ifdef CXBX_USE_VS30
//#define CXBX_USE_VS30 // Separate the port to Vertex Shader model 3.0 from the port to Direct3D9
#endif

#define DbgVshPrintf \
	LOG_CHECK_ENABLED(LOG_LEVEL::DEBUG) \
		if(g_bPrintfOn) printf

extern XTL::X_STREAMINPUT g_SetStreamSources[16]; // Declared in XbVertexBuffer.cpp, set by SetStreamSource

// Set by CxbxImpl_SetVertexShaderInput() :
int g_Xbox_SetVertexShaderInput_Count = 0;
XTL::X_STREAMINPUT g_Xbox_SetVertexShaderInput_Data[16] = { 0 }; // Active when g_Xbox_SetVertexShaderInput_Count > 0
XTL::X_VERTEXATTRIBUTEFORMAT g_Xbox_SetVertexShaderInput_Attributes = { 0 };

// Variables set by CxbxImpl_SetVertexShader() and CxbxImpl_SelectVertexShader() :
DWORD g_Xbox_VertexShader_Handle = 0;
DWORD g_Xbox_VertexShader_FunctionSlots_StartAddress = 0;

// Set by CxbxImpl_LoadVertexShader() / CxbxImpl_LoadVertexShaderProgram() :
DWORD g_Xbox_VertexShader_FunctionSlots[VSH_XBOX_MAX_INSTRUCTION_COUNT * 4] = { 0 }; // Each slot takes either 4 DWORDS (for instructions) or 4 floats (for constants)

// Set by CxbxLocateVertexShader()
DWORD* g_XboxAddr_pVertexShader = xbnullptr; // TODO : Get this symbol from g_SymbolAddresses["D3DDevice.m_pVertexShader"]; or whats its name

typedef uint16_t binary16_t; // Quick and dirty way to indicate IEEE754–2008 'half-precision floats'

// ****************************************************************************
// * Vertex shader function recompiler
// ****************************************************************************

typedef enum _VSH_SWIZZLE
{
	SWIZZLE_X = 0,
	SWIZZLE_Y,
	SWIZZLE_Z,
	SWIZZLE_W
}
VSH_SWIZZLE;

typedef struct DxbxSwizzles { VSH_SWIZZLE s[4]; } DxbxSwizzles;

typedef DWORD DxbxMask,
*PDxbxMask;

#define MASK_X 0x001
#define MASK_Y 0x002
#define MASK_Z 0x004
#define MASK_W 0x008
#define MASK_XYZ MASK_X | MASK_Y | MASK_Z
#define MASK_XYZW MASK_X | MASK_Y | MASK_Z | MASK_W

// Local types
typedef enum _VSH_FIELD_NAME
{
    FLD_ILU = 0,
    FLD_MAC,
    FLD_CONST,
    FLD_V,
    // Input A
    FLD_A_NEG,
    FLD_A_SWZ_X,
    FLD_A_SWZ_Y,
    FLD_A_SWZ_Z,
    FLD_A_SWZ_W,
    FLD_A_R,
    FLD_A_MUX,
    // Input B
    FLD_B_NEG,
    FLD_B_SWZ_X,
    FLD_B_SWZ_Y,
    FLD_B_SWZ_Z,
    FLD_B_SWZ_W,
    FLD_B_R,
    FLD_B_MUX,
    // Input C
    FLD_C_NEG,
    FLD_C_SWZ_X,
    FLD_C_SWZ_Y,
    FLD_C_SWZ_Z,
    FLD_C_SWZ_W,
    FLD_C_R_HIGH,
    FLD_C_R_LOW,
    FLD_C_MUX,
    // Output
    FLD_OUT_MAC_MASK_X,
    FLD_OUT_MAC_MASK_Y,
    FLD_OUT_MAC_MASK_Z,
    FLD_OUT_MAC_MASK_W,
    FLD_OUT_R,
    FLD_OUT_ILU_MASK_X,
    FLD_OUT_ILU_MASK_Y,
    FLD_OUT_ILU_MASK_Z,
    FLD_OUT_ILU_MASK_W,
    FLD_OUT_O_MASK_X,
    FLD_OUT_O_MASK_Y,
    FLD_OUT_O_MASK_Z,
    FLD_OUT_O_MASK_W,
    FLD_OUT_ORB,
    FLD_OUT_ADDRESS,
    FLD_OUT_MUX,
    // Relative addressing
    FLD_A0X,
    // Final instruction
    FLD_FINAL
}
VSH_FIELD_NAME;

typedef enum _VSH_OREG_NAME
{
	OREG_OPOS,    //  0
	OREG_UNUSED1, //  1
	OREG_UNUSED2, //  2
	OREG_OD0,     //  3
	OREG_OD1,     //  4
	OREG_OFOG,    //  5
	OREG_OPTS,    //  6
	OREG_OB0,     //  7
	OREG_OB1,     //  8
	OREG_OT0,     //  9
	OREG_OT1,     // 10
	OREG_OT2,     // 11
	OREG_OT3,     // 12
	OREG_UNUSED3, // 13
	OREG_UNUSED4, // 14
	OREG_A0X      // 15 - all values of the 4 bits are used
}
VSH_OREG_NAME;

typedef enum _VSH_OUTPUT_TYPE
{
    OUTPUT_C = 0,
    OUTPUT_O
}
VSH_OUTPUT_TYPE;

typedef enum _VSH_ARGUMENT_TYPE
{
    PARAM_UNKNOWN = 0,
    PARAM_R,          // Temporary registers
    PARAM_V,          // Vertex registers
    PARAM_C,          // Constant registers, set by SetVertexShaderConstant
    PARAM_O
}
VSH_ARGUMENT_TYPE;

typedef VSH_ARGUMENT_TYPE VSH_PARAMETER_TYPE; // Alias, to indicate difference between a parameter and a generic argument

typedef enum _VSH_OUTPUT_MUX
{
    OMUX_MAC = 0,
    OMUX_ILU
}
VSH_OUTPUT_MUX;

typedef enum _VSH_IMD_OUTPUT_TYPE
{
    IMD_OUTPUT_C,
    IMD_OUTPUT_R,
    IMD_OUTPUT_O,
    IMD_OUTPUT_A0X
}
VSH_IMD_OUTPUT_TYPE;

// Dxbx note : ILU stands for 'Inverse Logic Unit' opcodes
typedef enum _VSH_ILU
{
    ILU_NOP = 0,
    ILU_MOV,
    ILU_RCP,
    ILU_RCC,
    ILU_RSQ,
    ILU_EXP,
    ILU_LOG,
    ILU_LIT // = 7 - all values of the 3 bits are used
}
VSH_ILU;

// Dxbx note : MAC stands for 'Multiply And Accumulate' opcodes
typedef enum _VSH_MAC
{
    MAC_NOP = 0,
    MAC_MOV,
    MAC_MUL,
    MAC_ADD,
    MAC_MAD,
    MAC_DP3,
    MAC_DPH,
    MAC_DP4,
    MAC_DST,
    MAC_MIN,
    MAC_MAX,
    MAC_SLT,
    MAC_SGE,
    MAC_ARL
	// ??? 14
	// ??? 15 - 2 values of the 4 bits are undefined
}
VSH_MAC;

typedef struct _VSH_OPCODE_PARAMS
{
	// Dxbx Note : Since we split up g_OpCodeParams into g_OpCodeParams_ILU and g_OpCodeParams_MAC
	// the following two members aren't needed anymore :
	// VSH_ILU   ILU;
    // VSH_MAC   MAC;
    boolean   A;
    boolean   B;
    boolean   C;
}
VSH_OPCODE_PARAMS;

typedef struct _VSH_PARAMETER
{
    VSH_PARAMETER_TYPE  ParameterType;   // Parameter type, R, V or C
    boolean             Neg;             // TRUE if negated, FALSE if not
    VSH_SWIZZLE         Swizzle[4];      // The four swizzles
    int16_t             Address;         // Register address
}
VSH_PARAMETER;

typedef struct _VSH_OUTPUT
{
    // Output register
    VSH_OUTPUT_MUX      OutputMux;       // MAC or ILU used as output
	VSH_OUTPUT_TYPE     OutputType;      // C or O
    boolean             OutputMask[4];
    int16_t             OutputAddress;
    // MAC output R register
    boolean             MACRMask[4];
    boolean             MACRAddress;
    // ILU output R register
    boolean             ILURMask[4];
    boolean             ILURAddress;
}
VSH_OUTPUT;

// The raw, parsed shader instruction (can be many combined [paired] instructions)
typedef struct _VSH_SHADER_INSTRUCTION
{
    VSH_ILU       ILU;
    VSH_MAC       MAC;
    VSH_OUTPUT    Output;
    VSH_PARAMETER A;
    VSH_PARAMETER B;
    VSH_PARAMETER C;
    boolean       a0x;
}
VSH_SHADER_INSTRUCTION;

typedef enum _VSH_IMD_INSTRUCTION_TYPE
{
    IMD_MAC,
    IMD_ILU
}
VSH_IMD_INSTRUCTION_TYPE;

typedef struct _VSH_IMD_OUTPUT
{
    VSH_IMD_OUTPUT_TYPE Type;
    boolean             Mask[4];
    int16_t             Address;
}
VSH_IMD_OUTPUT;

typedef struct _VSH_IMD_PARAMETER
{
    boolean         Active;
    VSH_PARAMETER   Parameter;
	// There is only a single address register in Microsoft DirectX 8.0.
	// The address register, designated as a0.x, may be used as signed
	// integer offset in relative addressing into the constant register file.
	//     c[a0.x + n]
	boolean         IndexesWithA0_X;
}
VSH_IMD_PARAMETER;

typedef struct _VSH_INTERMEDIATE_FORMAT
{

    boolean                  IsCombined;
    VSH_IMD_INSTRUCTION_TYPE InstructionType;
    VSH_MAC                  MAC;
    VSH_ILU                  ILU;
    VSH_IMD_OUTPUT           Output;
    VSH_IMD_PARAMETER        Parameters[3];
}
VSH_INTERMEDIATE_FORMAT;

// Used for xvu spec definition
typedef struct _VSH_FIELDMAPPING
{
    VSH_FIELD_NAME  FieldName;
    uint8_t          SubToken;
    uint8_t          StartBit;
    uint8_t          BitLength;
}
VSH_FIELDMAPPING;

typedef struct _VSH_XBOX_SHADER
{
    XTL::X_VSH_SHADER_HEADER       ShaderHeader;
    uint16_t                IntermediateCount;
    VSH_INTERMEDIATE_FORMAT Intermediate[VSH_MAX_INTERMEDIATE_COUNT];
}
VSH_XBOX_SHADER;

// Local constants
static const VSH_FIELDMAPPING g_FieldMapping[] =
{
    // Field Name         DWORD BitPos BitSize
    {  FLD_ILU,              1,   25,     3 },
    {  FLD_MAC,              1,   21,     4 },
    {  FLD_CONST,            1,   13,     8 },
    {  FLD_V,                1,    9,     4 },
    // Input A
    {  FLD_A_NEG,            1,    8,     1 },
    {  FLD_A_SWZ_X,          1,    6,     2 },
    {  FLD_A_SWZ_Y,          1,    4,     2 },
    {  FLD_A_SWZ_Z,          1,    2,     2 },
    {  FLD_A_SWZ_W,          1,    0,     2 },
    {  FLD_A_R,              2,   28,     4 },
    {  FLD_A_MUX,            2,   26,     2 },
    // Input B
    {  FLD_B_NEG,            2,   25,     1 },
    {  FLD_B_SWZ_X,          2,   23,     2 },
    {  FLD_B_SWZ_Y,          2,   21,     2 },
    {  FLD_B_SWZ_Z,          2,   19,     2 },
    {  FLD_B_SWZ_W,          2,   17,     2 },
    {  FLD_B_R,              2,   13,     4 },
    {  FLD_B_MUX,            2,   11,     2 },
    // Input C
    {  FLD_C_NEG,            2,   10,     1 },
    {  FLD_C_SWZ_X,          2,    8,     2 },
    {  FLD_C_SWZ_Y,          2,    6,     2 },
    {  FLD_C_SWZ_Z,          2,    4,     2 },
    {  FLD_C_SWZ_W,          2,    2,     2 },
    {  FLD_C_R_HIGH,         2,    0,     2 },
    {  FLD_C_R_LOW,          3,   30,     2 },
    {  FLD_C_MUX,            3,   28,     2 },
    // Output
    {  FLD_OUT_MAC_MASK_X,   3,   27,     1 },
    {  FLD_OUT_MAC_MASK_Y,   3,   26,     1 },
    {  FLD_OUT_MAC_MASK_Z,   3,   25,     1 },
    {  FLD_OUT_MAC_MASK_W,   3,   24,     1 },
    {  FLD_OUT_R,            3,   20,     4 },
    {  FLD_OUT_ILU_MASK_X,   3,   19,     1 },
    {  FLD_OUT_ILU_MASK_Y,   3,   18,     1 },
    {  FLD_OUT_ILU_MASK_Z,   3,   17,     1 },
    {  FLD_OUT_ILU_MASK_W,   3,   16,     1 },
    {  FLD_OUT_O_MASK_X,     3,   15,     1 },
    {  FLD_OUT_O_MASK_Y,     3,   14,     1 },
    {  FLD_OUT_O_MASK_Z,     3,   13,     1 },
    {  FLD_OUT_O_MASK_W,     3,   12,     1 },
    {  FLD_OUT_ORB,          3,   11,     1 },
    {  FLD_OUT_ADDRESS,      3,    3,     8 },
    {  FLD_OUT_MUX,          3,    2,     1 },
	// Relative addressing
    {  FLD_A0X,              3,    1,     1 },
	// Final instruction
	{  FLD_FINAL,            3,    0,     1 }
};

static const VSH_OPCODE_PARAMS g_OpCodeParams_ILU[] =
{
	//  ILU OP   MAC OP      ParamA ParamB ParamC
	{ /*ILU_NOP, MAC_NOP, */ FALSE, FALSE, FALSE },  // Dxbx note : Unused
	{ /*ILU_MOV, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_RCP, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_RCC, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_RSQ, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_EXP, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_LOG, MAC_NOP, */ FALSE, FALSE, TRUE  },
	{ /*ILU_LIT, MAC_NOP, */ FALSE, FALSE, TRUE  },
};

static const VSH_OPCODE_PARAMS g_OpCodeParams_MAC[] =
{
	//  ILU OP   MAC OP      ParamA ParamB ParamC
	{ /*ILU_NOP, MAC_NOP, */ FALSE, FALSE, FALSE },  // Dxbx note : Unused
	{ /*ILU_NOP, MAC_MOV, */ TRUE,  FALSE, FALSE },
	{ /*ILU_NOP, MAC_MUL, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_ADD, */ TRUE,  FALSE, TRUE  },
    { /*ILU_NOP, MAC_MAD, */ TRUE,  TRUE,  TRUE  },
    { /*ILU_NOP, MAC_DP3, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_DPH, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_DP4, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_DST, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_MIN, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_MAX, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_SLT, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_SGE, */ TRUE,  TRUE,  FALSE },
    { /*ILU_NOP, MAC_ARL, */ TRUE,  FALSE, FALSE }
};

static const char* MAC_OpCode[] =
{
    "nop",
    "mov",
    "mul",
    "add",
    "mad",
    "dp3",
    "dph",
    "dp4",
    "dst",
    "min",
    "max",
    "slt",
    "sge",
    "mova", // really "arl" Dxbx note : Alias for 'mov a0.x'
    "???",
    "???"
};

static const char* ILU_OpCode[] =
{
    "nop",
    "mov",
    "rcp",
    "rcc",
    "rsq",
    "frc", // The Xbox EXP instruction behaves like FRC on PC, so we just translate it as frc. Fixes broken polygons in THPS2X
    "log",
    "lit"
};

static const char* OReg_Name[] =
{
    "oPos",
    "???",
    "???",
    "oD0",
    "oD1",
    "oFog",
    "oPts",
    "oB0",
    "oB1",
    "oT0",
    "oT1",
    "oT2",
    "oT3",
    "???",
    "???",
    "a0.x"
};

std::array<bool, 16> RegVIsPresentInDeclaration;
std::array<bool, 16> RegVIsUsedByShader;

/* TODO : map non-FVF Xbox vertex shader handle to CxbxVertexShader (a struct containing a host Xbox vertex shader handle and the original members)
std::unordered_map<DWORD, CxbxVertexShader> g_CxbxVertexShaders;

void CxbxUpdateVertexShader(DWORD XboxVertexShaderHandle)
{
	CxbxVertexShader &VertexShader = g_CxbxVertexShaders[XboxVertexShaderHandle];
}*/

static inline int IsInUse(const boolean *pMask)
{
    return (pMask[0] || pMask[1] || pMask[2] || pMask[3]);
}

static inline boolean HasMACR(VSH_SHADER_INSTRUCTION *pInstruction)
{
    return IsInUse(pInstruction->Output.MACRMask) && pInstruction->MAC != MAC_NOP;
}

static inline boolean HasMACO(VSH_SHADER_INSTRUCTION *pInstruction)
{
    return IsInUse(pInstruction->Output.OutputMask) &&
            pInstruction->Output.OutputMux == OMUX_MAC &&
            pInstruction->MAC != MAC_NOP;
}

static inline boolean HasMACARL(VSH_SHADER_INSTRUCTION *pInstruction)
{
    return /*!IsInUse(pInstruction->Output.OutputMask) &&
            pInstruction->Output.OutputMux == OMUX_MAC &&*/
            pInstruction->MAC == MAC_ARL;
}

static inline boolean HasILUR(VSH_SHADER_INSTRUCTION *pInstruction)
{
    return IsInUse(pInstruction->Output.ILURMask) && pInstruction->ILU != ILU_NOP;
}

static inline boolean HasILUO(VSH_SHADER_INSTRUCTION *pInstruction)
{
    return IsInUse(pInstruction->Output.OutputMask) &&
            pInstruction->Output.OutputMux == OMUX_ILU &&
            pInstruction->ILU != ILU_NOP;
}

// Retrieves a number of bits in the instruction token
static inline int VshGetFromToken(uint32_t *pShaderToken,
                                  uint8_t SubToken,
                                  uint8_t StartBit,
                                  uint8_t BitLength)
{
    return (pShaderToken[SubToken] >> StartBit) & ~(0xFFFFFFFF << BitLength);
}

// Converts the C register address to disassembly format
static inline int16_t ConvertCRegister(const int16_t CReg)
{
    return ((((CReg >> 5) & 7) - 3) * 32) + (CReg & 31);
}

uint8_t VshGetField(uint32_t         *pShaderToken,
                   VSH_FIELD_NAME FieldName)
{
    return (uint8_t)(VshGetFromToken(pShaderToken,
                                   g_FieldMapping[FieldName].SubToken,
                                   g_FieldMapping[FieldName].StartBit,
                                   g_FieldMapping[FieldName].BitLength));
}

static VSH_OPCODE_PARAMS* VshGetOpCodeParams(VSH_ILU ILU,
                                             VSH_MAC MAC)
{
	if (ILU >= ILU_MOV && ILU <= ILU_LIT)
		return (VSH_OPCODE_PARAMS*)&g_OpCodeParams_ILU[ILU];
	else
		if (MAC >= MAC_MOV && MAC <= MAC_ARL)
			return (VSH_OPCODE_PARAMS*)&g_OpCodeParams_MAC[MAC];
		else
		    return NULL;
}

static void VshParseInstruction(uint32_t               *pShaderToken,
                                VSH_SHADER_INSTRUCTION *pInstruction)
{
    // First get the instruction(s).
    pInstruction->ILU = (VSH_ILU)VshGetField(pShaderToken, FLD_ILU);
    pInstruction->MAC = (VSH_MAC)VshGetField(pShaderToken, FLD_MAC);

    // Get parameter A
    pInstruction->A.ParameterType = (VSH_PARAMETER_TYPE)VshGetField(pShaderToken, FLD_A_MUX);
    switch(pInstruction->A.ParameterType)
    {
    case PARAM_R:
        pInstruction->A.Address = VshGetField(pShaderToken, FLD_A_R);
        break;
    case PARAM_V:
        pInstruction->A.Address = VshGetField(pShaderToken, FLD_V);
        break;
    case PARAM_C:
        pInstruction->A.Address = ConvertCRegister(VshGetField(pShaderToken, FLD_CONST));
        break;
    default:
        EmuLog(LOG_LEVEL::WARNING, "Invalid instruction, parameter A type unknown %d", pInstruction->A.ParameterType);
        return;
    }
    pInstruction->A.Neg = VshGetField(pShaderToken, FLD_A_NEG);
    pInstruction->A.Swizzle[0] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_A_SWZ_X);
    pInstruction->A.Swizzle[1] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_A_SWZ_Y);
    pInstruction->A.Swizzle[2] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_A_SWZ_Z);
    pInstruction->A.Swizzle[3] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_A_SWZ_W);
    // Get parameter B
    pInstruction->B.ParameterType = (VSH_PARAMETER_TYPE)VshGetField(pShaderToken, FLD_B_MUX);
    switch(pInstruction->B.ParameterType)
    {
    case PARAM_R:
        pInstruction->B.Address = VshGetField(pShaderToken, FLD_B_R);
        break;
    case PARAM_V:
        pInstruction->B.Address = VshGetField(pShaderToken, FLD_V);
        break;
    case PARAM_C:
        pInstruction->B.Address = ConvertCRegister(VshGetField(pShaderToken, FLD_CONST));
        break;
    default:
        DbgVshPrintf("Invalid instruction, parameter B type unknown %d\n", pInstruction->B.ParameterType);
        return;
    }
    pInstruction->B.Neg = VshGetField(pShaderToken, FLD_B_NEG);
    pInstruction->B.Swizzle[0] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_B_SWZ_X);
    pInstruction->B.Swizzle[1] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_B_SWZ_Y);
    pInstruction->B.Swizzle[2] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_B_SWZ_Z);
    pInstruction->B.Swizzle[3] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_B_SWZ_W);
    // Get parameter C
    pInstruction->C.ParameterType = (VSH_PARAMETER_TYPE)VshGetField(pShaderToken, FLD_C_MUX);
    switch(pInstruction->C.ParameterType)
    {
    case PARAM_R:
        pInstruction->C.Address = VshGetField(pShaderToken, FLD_C_R_HIGH) << 2 |
                                  VshGetField(pShaderToken, FLD_C_R_LOW);
        break;
    case PARAM_V:
        pInstruction->C.Address = VshGetField(pShaderToken, FLD_V);
        break;
    case PARAM_C:
        pInstruction->C.Address = ConvertCRegister(VshGetField(pShaderToken, FLD_CONST));
        break;
    default:
        DbgVshPrintf("Invalid instruction, parameter C type unknown %d\n", pInstruction->C.ParameterType);
        return;
    }
    pInstruction->C.Neg = VshGetField(pShaderToken, FLD_C_NEG);
    pInstruction->C.Swizzle[0] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_C_SWZ_X);
    pInstruction->C.Swizzle[1] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_C_SWZ_Y);
    pInstruction->C.Swizzle[2] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_C_SWZ_Z);
    pInstruction->C.Swizzle[3] = (VSH_SWIZZLE)VshGetField(pShaderToken, FLD_C_SWZ_W);
    // Get output
    // Output register
    pInstruction->Output.OutputType = (VSH_OUTPUT_TYPE)VshGetField(pShaderToken, FLD_OUT_ORB);
    switch(pInstruction->Output.OutputType)
    {
    case OUTPUT_C:
        pInstruction->Output.OutputAddress = ConvertCRegister(VshGetField(pShaderToken, FLD_OUT_ADDRESS));
        break;
    case OUTPUT_O:
        pInstruction->Output.OutputAddress = VshGetField(pShaderToken, FLD_OUT_ADDRESS) & 0xF;
        break;
    }
    pInstruction->Output.OutputMux = (VSH_OUTPUT_MUX)VshGetField(pShaderToken, FLD_OUT_MUX);
    pInstruction->Output.OutputMask[0] = VshGetField(pShaderToken, FLD_OUT_O_MASK_X);
    pInstruction->Output.OutputMask[1] = VshGetField(pShaderToken, FLD_OUT_O_MASK_Y);
    pInstruction->Output.OutputMask[2] = VshGetField(pShaderToken, FLD_OUT_O_MASK_Z);
    pInstruction->Output.OutputMask[3] = VshGetField(pShaderToken, FLD_OUT_O_MASK_W);
    // MAC output
    pInstruction->Output.MACRMask[0] = VshGetField(pShaderToken, FLD_OUT_MAC_MASK_X);
    pInstruction->Output.MACRMask[1] = VshGetField(pShaderToken, FLD_OUT_MAC_MASK_Y);
    pInstruction->Output.MACRMask[2] = VshGetField(pShaderToken, FLD_OUT_MAC_MASK_Z);
    pInstruction->Output.MACRMask[3] = VshGetField(pShaderToken, FLD_OUT_MAC_MASK_W);
    pInstruction->Output.MACRAddress = VshGetField(pShaderToken, FLD_OUT_R);
    // ILU output
    pInstruction->Output.ILURMask[0] = VshGetField(pShaderToken, FLD_OUT_ILU_MASK_X);
    pInstruction->Output.ILURMask[1] = VshGetField(pShaderToken, FLD_OUT_ILU_MASK_Y);
    pInstruction->Output.ILURMask[2] = VshGetField(pShaderToken, FLD_OUT_ILU_MASK_Z);
    pInstruction->Output.ILURMask[3] = VshGetField(pShaderToken, FLD_OUT_ILU_MASK_W);
    pInstruction->Output.ILURAddress = VshGetField(pShaderToken, FLD_OUT_R);
    // Finally, get a0.x indirect constant addressing
    pInstruction->a0x = VshGetField(pShaderToken, FLD_A0X);
}

// Print functions
static char *VshGetRegisterName(VSH_PARAMETER_TYPE ParameterType)
{
    switch(ParameterType)
    {
    case PARAM_R:
        return "r";
    case PARAM_V:
        return "v";
    case PARAM_C:
        return "c";
	case PARAM_O:
		return "oPos";
	default:
        return "?";
    }
}

static void VshWriteOutputMask(boolean *OutputMask,
                               std::stringstream& pDisassembly)
{
    if(OutputMask[0] && OutputMask[1] && OutputMask[2] && OutputMask[3])
    {
        // All components are there, no need to print the mask
        return;
    }
	pDisassembly << "." << (OutputMask[0] ? "x" : "")
						<< (OutputMask[1] ? "y" : "")
						<< (OutputMask[2] ? "z" : "")
						<< (OutputMask[3] ? "w" : "");
}

static void VshWriteParameter(VSH_IMD_PARAMETER *pParameter,
                              std::stringstream& pDisassembly)
{
    pDisassembly << ", " << (pParameter->Parameter.Neg ? "-" : "") << VshGetRegisterName(pParameter->Parameter.ParameterType);
    if(pParameter->Parameter.ParameterType == PARAM_C && pParameter->IndexesWithA0_X)
    {
        // Only display the offset if it's not 0.
        if(pParameter->Parameter.Address)
        {
			pDisassembly << "[a0.x+" << pParameter->Parameter.Address << "]";
        }
        else
        {
			pDisassembly << "[a0.x]";
        }
    }
    else
    {
		pDisassembly << pParameter->Parameter.Address;
    }
    // Only bother printing the swizzle if it is not .xyzw
    if(!(pParameter->Parameter.Swizzle[0] == SWIZZLE_X &&
         pParameter->Parameter.Swizzle[1] == SWIZZLE_Y &&
         pParameter->Parameter.Swizzle[2] == SWIZZLE_Z &&
         pParameter->Parameter.Swizzle[3] == SWIZZLE_W))
    {
        int i;

		pDisassembly << ".";
        for (i = 0; i < 4; i++)
        {
            int j;
            char Swizzle = '?';
            switch(pParameter->Parameter.Swizzle[i])
            {
            case SWIZZLE_X:
                Swizzle = 'x';
                break;
            case SWIZZLE_Y:
                Swizzle = 'y';
                break;
            case SWIZZLE_Z:
                Swizzle = 'z';
                break;
            case SWIZZLE_W:
                Swizzle = 'w';
                break;
            }
			pDisassembly << Swizzle;
            for (j = i; j < 4; j++)
            {
                if(pParameter->Parameter.Swizzle[i] != pParameter->Parameter.Swizzle[j])
                {
                    break;
                }
            }
            if(j == 4)
            {
                break;
            }
        }
    }
}


#define D3DDECLUSAGE_UNSUPPORTED ((D3DDECLUSAGE)-1)

XTL::D3DDECLUSAGE Xb2PCRegisterType
(
	DWORD VertexRegister,
	BYTE& PCUsageIndex
)
{
	using namespace XTL;

	D3DDECLUSAGE PCRegisterType;
	PCUsageIndex = 0;

	switch (VertexRegister)
	{
	case X_D3DVSDE_VERTEX: // -1
		DbgVshPrintf("D3DVSDE_VERTEX /* xbox ext. */");
		PCRegisterType = D3DDECLUSAGE_UNSUPPORTED;
		break;
	case X_D3DVSDE_POSITION: // 0
		DbgVshPrintf("D3DVSDE_POSITION");
		PCRegisterType = D3DDECLUSAGE_POSITION;
		break;
	case X_D3DVSDE_BLENDWEIGHT: // 1
		DbgVshPrintf("D3DVSDE_BLENDWEIGHT");
		PCRegisterType = D3DDECLUSAGE_BLENDWEIGHT;
		break;
	case X_D3DVSDE_NORMAL: // 2
		DbgVshPrintf("D3DVSDE_NORMAL");
		PCRegisterType = D3DDECLUSAGE_NORMAL;
		break;
	case X_D3DVSDE_DIFFUSE: // 3
		DbgVshPrintf("D3DVSDE_DIFFUSE");
		PCRegisterType = D3DDECLUSAGE_COLOR; PCUsageIndex = 0;
		break;
	case X_D3DVSDE_SPECULAR: // 4
		DbgVshPrintf("D3DVSDE_SPECULAR");
		PCRegisterType = D3DDECLUSAGE_COLOR; PCUsageIndex = 1;
		break;
	case X_D3DVSDE_FOG: // 5
		DbgVshPrintf("D3DVSDE_FOG");
		PCRegisterType = D3DDECLUSAGE_FOG;
		break;
	case X_D3DVSDE_POINTSIZE: // 6
		DbgVshPrintf("D3DVDSE_POINTSIZE");
		PCRegisterType = D3DDECLUSAGE_PSIZE;
		break;
	case X_D3DVSDE_BACKDIFFUSE: // 7
		DbgVshPrintf("D3DVSDE_BACKDIFFUSE /* xbox ext. */");
		PCRegisterType = D3DDECLUSAGE_COLOR; PCUsageIndex = 2;
		break;
	case X_D3DVSDE_BACKSPECULAR: // 8
		DbgVshPrintf("D3DVSDE_BACKSPECULAR /* xbox ext. */");
		PCRegisterType = D3DDECLUSAGE_COLOR; PCUsageIndex = 3;
		break;
	case X_D3DVSDE_TEXCOORD0: // 9
		DbgVshPrintf("D3DVSDE_TEXCOORD0");
		PCRegisterType = D3DDECLUSAGE_TEXCOORD; PCUsageIndex = 0;
		break;
	case X_D3DVSDE_TEXCOORD1: // 10
		DbgVshPrintf("D3DVSDE_TEXCOORD1");
		PCRegisterType = D3DDECLUSAGE_TEXCOORD; PCUsageIndex = 1;
		break;
	case X_D3DVSDE_TEXCOORD2: // 11
		DbgVshPrintf("D3DVSDE_TEXCOORD2");
		PCRegisterType = D3DDECLUSAGE_TEXCOORD; PCUsageIndex = 2;
		break;
	case X_D3DVSDE_TEXCOORD3: // 12
		DbgVshPrintf("D3DVSDE_TEXCOORD3");
		PCRegisterType = D3DDECLUSAGE_TEXCOORD; PCUsageIndex = 3;
		break;
	default:
		DbgVshPrintf("%d /* unknown register */", VertexRegister);
		PCRegisterType = D3DDECLUSAGE_UNSUPPORTED;
		break;
	}

	return PCRegisterType;
}

extern XTL::D3DCAPS g_D3DCaps;

static void VshWriteShader(VSH_XBOX_SHADER *pShader,
                           std::stringstream& pDisassembly,
						   XTL::D3DVERTEXELEMENT *pRecompiled,
                           boolean Truncate)
{
    switch(pShader->ShaderHeader.Version)
    {
        case VERSION_VS:
#ifdef CXBX_USE_VS30
			pDisassembly << "vs.3.0\n";
#else
			pDisassembly << "vs.2.x\n";
#endif
            break;
        case VERSION_XVS:
			pDisassembly << "xvs.1.1\n";
            break;
        case VERSION_XVSS:
			pDisassembly << "xvss.1.1\n";
            break;
        case VERSION_XVSW:
			pDisassembly << "xvsw.1.1\n";
            break;
        default:
            break;
    }

	if (Truncate) {
		std::stringstream moveConstantsToTemporaries;

		pDisassembly << "; Input usage declarations --\n";
		unsigned i = 0;
		do {
			if (RegVIsUsedByShader[i]) {
				if (!RegVIsPresentInDeclaration[i]) {
					// Log test case and skip
					// Any registers hitting this critera were already replaced with constant/temporary reads
					// To correctly use the values given in SetVertexData4f.
					// We need to move these constant values to temporaries so they can be used as input alongside other constants!
					// We count down from the highest available on the host because Xbox titles don't use values that high, and we read from c192 (one above maximum Xbox c191 constant) and up
					static int temporaryRegisterBase = g_D3DCaps.VS20Caps.NumTemps - 13;
					moveConstantsToTemporaries << "mov r" << (temporaryRegisterBase + i) << ", c" << (X_D3DVS_CONSTREG_VERTEXDATA4F_BASE + i) << "\n";
					LOG_TEST_CASE("Shader uses undeclared Vertex Input Registers");
					i++;
					continue;
				}

				// dcl_texcoord can be useds for any user-defined data
				// We need this because there is no reliable way to detect the real usage
				// Xbox has no concept of 'usage types', it only requires a list of attribute register numbers.
				// So we treat them all as 'user-defined'
				pDisassembly << "dcl_texcoord" << i << " v" << i << "\n";
			}

			i++;
		} while (i < RegVIsUsedByShader.size());

		pDisassembly << moveConstantsToTemporaries.str();
	}

    for (int i = 0; i < pShader->IntermediateCount && (i < VSH_MAX_INSTRUCTION_COUNT || !Truncate); i++)
    {
        VSH_INTERMEDIATE_FORMAT *pIntermediate = &pShader->Intermediate[i];

        if(i == VSH_MAX_INSTRUCTION_COUNT)
        {
			pDisassembly << "; -- Passing the truncation limit --\n";
        }

        // Writing combining sign if neccessary
        if(pIntermediate->IsCombined)
        {
			pDisassembly << "+";
        }

        // Print the op code
        if(pIntermediate->InstructionType == IMD_MAC)
        {
			// Dxbx addition : Safeguard against incorrect MAC opcodes :
			if (pIntermediate->MAC > MAC_ARL)
				pDisassembly << "??? ";
			else
				pDisassembly << MAC_OpCode[pIntermediate->MAC] << " ";
        }
        else // IMD_ILU
        {
			// Dxbx addition : Safeguard against incorrect ILU opcodes :
			if (pIntermediate->ILU > ILU_LIT)
				pDisassembly << "??? ";
			else
				pDisassembly << ILU_OpCode[pIntermediate->ILU] << " ";
        }

        // Print the output parameter
        if(pIntermediate->Output.Type == IMD_OUTPUT_A0X)
        {
			pDisassembly << "a0.x";
        }
        else
        {
            switch(pIntermediate->Output.Type)
            {
            case IMD_OUTPUT_C:
				pDisassembly << "c" << pIntermediate->Output.Address;
                break;
            case IMD_OUTPUT_R:
				pDisassembly << "r" << pIntermediate->Output.Address;
                break;
            case IMD_OUTPUT_O:
				// Dxbx addition : Safeguard against incorrect VSH_OREG_NAME values :
				if ((int)pIntermediate->Output.Address > OREG_A0X)
					; // don't add anything
				else
					pDisassembly << OReg_Name[pIntermediate->Output.Address];
                break;
            default:
                CxbxKrnlCleanup("Invalid output register in vertex shader!");
                break;
            }
            VshWriteOutputMask(pIntermediate->Output.Mask, pDisassembly);
        }
        // Print the parameters
        for (int p = 0; p < 3; p++)
        {
            VSH_IMD_PARAMETER *pParameter = &pIntermediate->Parameters[p];
            if(pParameter->Active)
            {
                VshWriteParameter(pParameter, pDisassembly);
            }
        }
		pDisassembly << "\n";
    }
}

static void VshAddParameter(VSH_PARAMETER     *pParameter,
                            boolean           a0x,
                            VSH_IMD_PARAMETER *pIntermediateParameter)
{
    pIntermediateParameter->Parameter = *pParameter;
    pIntermediateParameter->Active    = TRUE;
    pIntermediateParameter->IndexesWithA0_X     = a0x;
}

static void VshAddParameters(VSH_SHADER_INSTRUCTION  *pInstruction,
                             VSH_ILU                 ILU,
                             VSH_MAC                 MAC,
                             VSH_IMD_PARAMETER       *pParameters)
{
    uint8_t ParamCount = 0;
    VSH_OPCODE_PARAMS* pParams = VshGetOpCodeParams(ILU, MAC);

    // param A
    if(pParams->A)
    {
        VshAddParameter(&pInstruction->A, pInstruction->a0x, &pParameters[ParamCount]);
        ParamCount++;
    }

    // param B
    if(pParams->B)
    {
        VshAddParameter(&pInstruction->B, pInstruction->a0x, &pParameters[ParamCount]);
        ParamCount++;
    }

    // param C
    if(pParams->C)
    {
        VshAddParameter(&pInstruction->C, pInstruction->a0x, &pParameters[ParamCount]);
        ParamCount++;
    }
}

static void VshVerifyBufferBounds(VSH_XBOX_SHADER *pShader)
{
    if(pShader->IntermediateCount == VSH_MAX_INTERMEDIATE_COUNT)
    {
        CxbxKrnlCleanup("Shader exceeds conversion buffer!");
    }
}

static VSH_INTERMEDIATE_FORMAT *VshNewIntermediate(VSH_XBOX_SHADER *pShader)
{
    VshVerifyBufferBounds(pShader);

    ZeroMemory(&pShader->Intermediate[pShader->IntermediateCount], sizeof(VSH_INTERMEDIATE_FORMAT));

    return &pShader->Intermediate[pShader->IntermediateCount++];
}

static void VshInsertIntermediate(VSH_XBOX_SHADER         *pShader,
                                  VSH_INTERMEDIATE_FORMAT *pIntermediate,
                                  uint16_t                 Pos)
{
    VshVerifyBufferBounds(pShader);

    for (int i = pShader->IntermediateCount; i >= Pos; i--)
    {
        pShader->Intermediate[i + 1] = pShader->Intermediate[i];
    }
    pShader->Intermediate[Pos] = *pIntermediate;
    pShader->IntermediateCount++;
}

static void VshDeleteIntermediate(VSH_XBOX_SHADER *pShader,
                                  uint16_t         Pos)
{
    for (int i = Pos; i < (pShader->IntermediateCount - 1); i++)
    {
        pShader->Intermediate[i] = pShader->Intermediate[i + 1];
    }
    pShader->IntermediateCount--;
}

static boolean VshAddInstructionMAC_R(VSH_SHADER_INSTRUCTION *pInstruction,
                                      VSH_XBOX_SHADER        *pShader,
                                      boolean                IsCombined)
{
    VSH_INTERMEDIATE_FORMAT *pIntermediate;
    if(!HasMACR(pInstruction))
    {
        return FALSE;
    }

    pIntermediate = VshNewIntermediate(pShader);
    pIntermediate->IsCombined = IsCombined;

    // Opcode
    pIntermediate->InstructionType = IMD_MAC;
    pIntermediate->MAC = pInstruction->MAC;

    // Output param
    pIntermediate->Output.Type = IMD_OUTPUT_R;
    pIntermediate->Output.Address = pInstruction->Output.MACRAddress;
    memcpy(pIntermediate->Output.Mask, pInstruction->Output.MACRMask, sizeof(boolean) * 4);

    // Other parameters
    VshAddParameters(pInstruction, ILU_NOP, pInstruction->MAC, pIntermediate->Parameters);

    return TRUE;
}

static boolean VshAddInstructionMAC_O(VSH_SHADER_INSTRUCTION* pInstruction,
                                      VSH_XBOX_SHADER        *pShader,
                                      boolean                IsCombined)
{
    VSH_INTERMEDIATE_FORMAT *pIntermediate;
    if(!HasMACO(pInstruction))
    {
        return FALSE;
    }

    pIntermediate = VshNewIntermediate(pShader);
    pIntermediate->IsCombined = IsCombined;

    // Opcode
    pIntermediate->InstructionType = IMD_MAC;
    pIntermediate->MAC = pInstruction->MAC;

    // Output param
	pIntermediate->Output.Type = pInstruction->Output.OutputType == OUTPUT_C ? IMD_OUTPUT_C : IMD_OUTPUT_O;
    pIntermediate->Output.Address = pInstruction->Output.OutputAddress;
    memcpy(pIntermediate->Output.Mask, pInstruction->Output.OutputMask, sizeof(boolean) * 4);

    // Other parameters
    VshAddParameters(pInstruction, ILU_NOP, pInstruction->MAC, pIntermediate->Parameters);

    return TRUE;
}

static boolean VshAddInstructionMAC_ARL(VSH_SHADER_INSTRUCTION *pInstruction,
                                        VSH_XBOX_SHADER        *pShader,
                                        boolean                IsCombined)
{
    VSH_INTERMEDIATE_FORMAT *pIntermediate;
    if(!HasMACARL(pInstruction))
    {
        return FALSE;
    }

    pIntermediate = VshNewIntermediate(pShader);
    pIntermediate->IsCombined = IsCombined;

    // Opcode
    pIntermediate->InstructionType = IMD_MAC;
    pIntermediate->MAC = pInstruction->MAC;

    // Output param
    pIntermediate->Output.Type = IMD_OUTPUT_A0X;
    pIntermediate->Output.Address = pInstruction->Output.OutputAddress;

    // Other parameters
    VshAddParameters(pInstruction, ILU_NOP, pInstruction->MAC, pIntermediate->Parameters);

    return TRUE;
}

// Dxbx addition : Scalar instructions reading from W should read from X instead
static boolean DxbxFixupScalarParameter(VSH_SHADER_INSTRUCTION *pInstruction,
    VSH_XBOX_SHADER *pShader,
    VSH_PARAMETER *pParameter);

static boolean VshAddInstructionILU_R(VSH_SHADER_INSTRUCTION *pInstruction,
                                      VSH_XBOX_SHADER        *pShader,
                                      boolean                IsCombined)
{
    VSH_INTERMEDIATE_FORMAT *pIntermediate;
    if(!HasILUR(pInstruction))
    {
        return FALSE;
    }

	pIntermediate = VshNewIntermediate(pShader);
    pIntermediate->IsCombined = IsCombined;

    // Opcode
    pIntermediate->InstructionType = IMD_ILU;
    pIntermediate->ILU = pInstruction->ILU;

    // Output param
    pIntermediate->Output.Type = IMD_OUTPUT_R;
    // If this is a combined instruction, only r1 is allowed (R address should not be used)
    pIntermediate->Output.Address = IsCombined ? 1 : pInstruction->Output.ILURAddress;
    memcpy(pIntermediate->Output.Mask, pInstruction->Output.ILURMask, sizeof(boolean) * 4);

    // Other parameters
    VshAddParameters(pInstruction, pInstruction->ILU, MAC_NOP, pIntermediate->Parameters);

    return TRUE;
}

static boolean VshAddInstructionILU_O(VSH_SHADER_INSTRUCTION *pInstruction,
                                      VSH_XBOX_SHADER        *pShader,
                                      boolean                IsCombined)
{
    VSH_INTERMEDIATE_FORMAT *pIntermediate;
    if(!HasILUO(pInstruction))
    {
        return FALSE;
    }

    pIntermediate = VshNewIntermediate(pShader);
    pIntermediate->IsCombined = IsCombined;

    // Opcode
    pIntermediate->InstructionType = IMD_ILU;
    pIntermediate->ILU = pInstruction->ILU;

    // Output param
    pIntermediate->Output.Type = pInstruction->Output.OutputType == OUTPUT_C ? IMD_OUTPUT_C : IMD_OUTPUT_O;
    pIntermediate->Output.Address = pInstruction->Output.OutputAddress;
    memcpy(pIntermediate->Output.Mask, pInstruction->Output.OutputMask, sizeof(boolean) * 4);

    // Other parameters
    VshAddParameters(pInstruction, pInstruction->ILU, MAC_NOP, pIntermediate->Parameters);

    return TRUE;
}

static void VshConvertToIntermediate(VSH_SHADER_INSTRUCTION *pInstruction,
                                     VSH_XBOX_SHADER        *pShader)
{
    // Five types of instructions:
    //   MAC
    //
    //   ILU
    //
    //   MAC
    //   +ILU
    //
    //   MAC
    //   +MAC
    //   +ILU
    //
    //   MAC
    //   +ILU
    //   +ILU
    boolean IsCombined = FALSE;

    // Dxbx note : Scalar instructions read from C, but use X instead of W, fix that :
    DxbxFixupScalarParameter(pInstruction, pShader, &pInstruction->C);

    if(VshAddInstructionMAC_R(pInstruction, pShader, IsCombined))
    {
        if(HasMACO(pInstruction) ||
            HasILUR(pInstruction) ||
            HasILUO(pInstruction))
        {
            IsCombined = TRUE;
        }
    }
    if(VshAddInstructionMAC_O(pInstruction, pShader, IsCombined))
    {
        if(HasILUR(pInstruction) ||
            HasILUO(pInstruction))
        {
            IsCombined = TRUE;
        }
    }
    // Special case, arl (mov a0.x, ...)
    if(VshAddInstructionMAC_ARL(pInstruction, pShader, IsCombined))
    {
        if(HasILUR(pInstruction) ||
            HasILUO(pInstruction))
        {
            IsCombined = TRUE;
        }
    }
    if(VshAddInstructionILU_R(pInstruction, pShader, IsCombined))
    {
        if(HasILUO(pInstruction))
        {
            IsCombined = TRUE;
        }
    }
    (void)VshAddInstructionILU_O(pInstruction, pShader, IsCombined);
}

static inline void VshSetSwizzle(VSH_PARAMETER *pParameter,
    VSH_SWIZZLE       x,
    VSH_SWIZZLE       y,
    VSH_SWIZZLE       z,
    VSH_SWIZZLE       w)
{
    pParameter->Swizzle[0] = x;
    pParameter->Swizzle[1] = y;
    pParameter->Swizzle[2] = z;
    pParameter->Swizzle[3] = w;
}

static inline void VshSetSwizzle(VSH_IMD_PARAMETER *pParameter,
                                 VSH_SWIZZLE       x,
                                 VSH_SWIZZLE       y,
                                 VSH_SWIZZLE       z,
                                 VSH_SWIZZLE       w)
{
    VshSetSwizzle(&pParameter->Parameter, x, y, z, w);
}

static inline void VshSetOutputMask(VSH_IMD_OUTPUT* pOutput,
                                    boolean MaskX,
                                    boolean MaskY,
                                    boolean MaskZ,
                                    boolean MaskW)
{
    pOutput->Mask[0] = MaskX;
    pOutput->Mask[1] = MaskY;
    pOutput->Mask[2] = MaskZ;
    pOutput->Mask[3] = MaskW;
}

// Dxbx addition : Scalar instructions reading from W should read from X instead
static boolean DxbxFixupScalarParameter(VSH_SHADER_INSTRUCTION *pInstruction,
    VSH_XBOX_SHADER *pShader,
    VSH_PARAMETER *pParameter)
{
    boolean Result;

    // The DirectX vertex shader language specifies that the exp, log, rcc, rcp, and rsq instructions
    // all operate on the "w" component of the input. But the microcode versions of these instructions
    // actually operate on the "x" component of the input.
    Result = false;

    // Test if this is a scalar instruction :
    if (pInstruction->ILU == ILU_RCP ||
        pInstruction->ILU == ILU_RCC ||
        pInstruction->ILU == ILU_RSQ ||
        pInstruction->ILU == ILU_LOG)
    {
        // Test if this parameter reads all components, including W (TODO : Or should we fixup any W reading swizzle?) :
        if ((pParameter->Swizzle[0] == SWIZZLE_X)
            && (pParameter->Swizzle[1] == SWIZZLE_Y)
            && (pParameter->Swizzle[2] == SWIZZLE_Z)
            && (pParameter->Swizzle[3] == SWIZZLE_W))
        {
            // Change the read from W into a read from X (this fixes the XDK VolumeLight sample) :
            VshSetSwizzle(pParameter, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X);
            DbgVshPrintf("Dxbx fixup on scalar instruction applied; Changed read of uninitialized W into a read of X!\n");
            Result = true;
        }
    }

    return Result;
}

/*
    mul oPos.xyz, r12, c-38
    +rcc r1.x, r12.w

    mad oPos.xyz, r12, r1.x, c-37
*/
static void VshRemoveScreenSpaceInstructions(VSH_XBOX_SHADER *pShader)
{
    int16_t PosC38    = -1;
    int deleted     = 0;

    for (int i = 0; i < pShader->IntermediateCount; i++)
    {
        VSH_INTERMEDIATE_FORMAT* pIntermediate = &pShader->Intermediate[i];

        for (int k = 0; k < 3; k++)
        {
            if(pIntermediate->Parameters[k].Active)
            {
                if(pIntermediate->Parameters[k].Parameter.ParameterType == PARAM_C &&
                   !pIntermediate->Parameters[k].IndexesWithA0_X)
                {
                    if(pIntermediate->Parameters[k].Parameter.Address == -37)
                    {
                        // Found c-37, remove the instruction
                        if(k == 2 &&
                           pIntermediate->Parameters[1].Active &&
                           pIntermediate->Parameters[1].Parameter.ParameterType == PARAM_R)
                        {
                            DbgVshPrintf("PosC38 = %d i = %d\n", PosC38, i);
                            for (int j = (i-1); j >= 0; j--)
                            {
                                VSH_INTERMEDIATE_FORMAT* pIntermediate1W = &pShader->Intermediate[j];
                                // Time to start searching for +rcc r#.x, r12.w
                                if(pIntermediate1W->InstructionType == IMD_ILU &&
                                    pIntermediate1W->ILU == ILU_RCC &&
                                    pIntermediate1W->Output.Type == IMD_OUTPUT_R &&
                                    pIntermediate1W->Output.Address ==
                                    pIntermediate->Parameters[1].Parameter.Address)
                                {
                                    DbgVshPrintf("Deleted +rcc r1.x, r12.w\n");
                                    VshDeleteIntermediate(pShader, j);
                                    deleted++;
                                    i--;
                                    //j--;
                                    break;
                                }
                            }
                        }
                        VshDeleteIntermediate(pShader, i);
                        deleted++;
                        i--;
                        DbgVshPrintf("Deleted mad oPos.xyz, r12, r1.x, c-37\n");
                        break;
                    }
                    else if(pIntermediate->Parameters[k].Parameter.Address == -38)
                    {
                        VshDeleteIntermediate(pShader, i);
                        PosC38 = i;
                        deleted++;
                        i--;
                        DbgVshPrintf("Deleted mul oPos.xyz, r12, c-38\n");
                    }
                }
            }
        }
    }

    // If we couldn't find the generic screen space transformation we're
    // assuming that the shader writes direct screen coordinates that must be
    // normalized. This hack will fail if (a) the shader uses custom screen
    // space transformation, (b) reads r13 or r12 after we have written to
    // them, or (c) doesn't reserve c-38 and c-37 for scale and offset.
    if(deleted != 3)
    {
        EmuLog(LOG_LEVEL::WARNING, "Applying screen space vertex shader patching hack!");
        for (int i = 0; i < pShader->IntermediateCount; i++)
        {
            VSH_INTERMEDIATE_FORMAT* pIntermediate = &pShader->Intermediate[i];

            // Find instructions outputting to oPos.
            if( pIntermediate->Output.Type    == IMD_OUTPUT_O &&
                pIntermediate->Output.Address == OREG_OPOS)
            {
                // Redirect output to r12.
                pIntermediate->Output.Type    = IMD_OUTPUT_R;
                pIntermediate->Output.Address = 12;

                // Scale r12 to r13. (mul r13.[mask], r12, c58)
                VSH_INTERMEDIATE_FORMAT MulIntermediate;
                MulIntermediate.IsCombined        = FALSE;
                MulIntermediate.InstructionType   = IMD_MAC;
                MulIntermediate.MAC               = MAC_MUL;
                MulIntermediate.Output.Type       = IMD_OUTPUT_R;
                MulIntermediate.Output.Address    = 13;
                MulIntermediate.Output.Mask[0]    = pIntermediate->Output.Mask[0];
                MulIntermediate.Output.Mask[1]    = pIntermediate->Output.Mask[1];
                MulIntermediate.Output.Mask[2]    = pIntermediate->Output.Mask[2];
                MulIntermediate.Output.Mask[3]    = pIntermediate->Output.Mask[3];
                MulIntermediate.Parameters[0].Active                  = TRUE;
                MulIntermediate.Parameters[0].IndexesWithA0_X                   = FALSE;
                MulIntermediate.Parameters[0].Parameter.ParameterType = PARAM_R;
                MulIntermediate.Parameters[0].Parameter.Address       = 12;
                MulIntermediate.Parameters[0].Parameter.Neg           = FALSE;
                VshSetSwizzle(&MulIntermediate.Parameters[0], SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W);
                MulIntermediate.Parameters[1].Active                  = TRUE;
                MulIntermediate.Parameters[1].IndexesWithA0_X                   = FALSE;
                MulIntermediate.Parameters[1].Parameter.ParameterType = PARAM_C;
                MulIntermediate.Parameters[1].Parameter.Address       = ConvertCRegister(58);
                MulIntermediate.Parameters[1].Parameter.Neg           = FALSE;
                VshSetSwizzle(&MulIntermediate.Parameters[1], SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W);
                MulIntermediate.Parameters[2].Active                  = FALSE;
                VshInsertIntermediate(pShader, &MulIntermediate, ++i);

                // Add offset with r13 to oPos (add oPos.[mask], r13, c59)
                VSH_INTERMEDIATE_FORMAT AddIntermediate = MulIntermediate;
                AddIntermediate.MAC               = MAC_ADD;
                AddIntermediate.Output.Type       = IMD_OUTPUT_O;
                AddIntermediate.Output.Address    = OREG_OPOS;
                AddIntermediate.Parameters[0].Parameter.ParameterType = PARAM_R;
                AddIntermediate.Parameters[0].Parameter.Address       = 13;
                AddIntermediate.Parameters[1].Parameter.Address       = ConvertCRegister(59);
                VshInsertIntermediate(pShader, &AddIntermediate, ++i);
            }
        }
    }
}

static void VshRemoveUnsupportedObRegisters(VSH_XBOX_SHADER *pShader)
{
	int deleted = 0;

	for (int i = 0; i < pShader->IntermediateCount; i++) {
		VSH_INTERMEDIATE_FORMAT* pIntermediate = &pShader->Intermediate[i];

		if (pIntermediate->Output.Type == IMD_OUTPUT_O && (pIntermediate->Output.Address == OREG_OB0 || pIntermediate->Output.Address == OREG_OB1)) {
			DbgVshPrintf("Deleted unsupported write to %s\n", OReg_Name[pIntermediate->Output.Address]);
			VshDeleteIntermediate(pShader, i);
			i--;
		}
	}
}

// Converts the intermediate format vertex shader to DirectX 8 format
static boolean VshConvertShader(VSH_XBOX_SHADER *pShader,
                                boolean         bNoReservedConstants
)
{
    using namespace XTL;

    const DWORD temporaryCount = g_D3DCaps.VS20Caps.NumTemps;

    boolean RUsage[VSH_MAX_TEMPORARY_REGISTERS] = { FALSE };
	// Set the last 13 register to used (they are used for SetVertexData4f Constants)
	for (int i = 1; i <= 13;  i++) {
		RUsage[VSH_MAX_TEMPORARY_REGISTERS - i] = true;
	}


    // TODO: What about state shaders and such?
    pShader->ShaderHeader.Version = VERSION_VS;

    // Search for the screen space instructions, and remove them
    if(!bNoReservedConstants)
    {
        VshRemoveScreenSpaceInstructions(pShader);
    }

	// Windows does not support back-facing colours, so we remove them from the shaders
	// Test Case: Panzer Dragoon Orta
	VshRemoveUnsupportedObRegisters(pShader);

    // TODO: Add routine for compacting r register usage so that at least one is freed (two if dph and r12)

    for (int i = 0; i < pShader->IntermediateCount; i++)
    {
        VSH_INTERMEDIATE_FORMAT* pIntermediate = &pShader->Intermediate[i];
        // Combining not supported in vs.1.1
        pIntermediate->IsCombined = FALSE;

        if(pIntermediate->Output.Type == IMD_OUTPUT_O && (pIntermediate->Output.Address == OREG_OPTS || pIntermediate->Output.Address == OREG_OFOG))
        {
            // The PC shader assembler doesn't like masks on scalar registers
			VshSetOutputMask(&pIntermediate->Output, TRUE, TRUE, TRUE, TRUE);

			// Fix when mad or mov to a scaler input does not use a replicate swizzle
			// MAD Test case: Panzer Dragoon Orta
			// MOV Test case: DOA3, Mechassault (Const)
			// MUL Test case: Amped
			// TODO Previously we applied this fix for specified instructions
			// When should we not apply the correction?
			if (true)
			{
				// Clear all but the first swizzle for each parameter
				// TODO: Is this sufficient? Perhaps we need to be smart about which swizzle to select
				for (int param = 0; param < 3; param++) {
					pIntermediate->Parameters[param].Parameter.Swizzle[1] = pIntermediate->Parameters[param].Parameter.Swizzle[0];
					pIntermediate->Parameters[param].Parameter.Swizzle[2] = pIntermediate->Parameters[param].Parameter.Swizzle[0];
					pIntermediate->Parameters[param].Parameter.Swizzle[3] = pIntermediate->Parameters[param].Parameter.Swizzle[0];
				}
			}
        }

        if(pIntermediate->InstructionType == IMD_ILU && pIntermediate->ILU == ILU_RCC)
        {
            // Convert rcc to rcp
            pIntermediate->ILU = ILU_RCP;
        }

        auto sw = pIntermediate->Parameters[0].Parameter.Swizzle;
        bool singleSwizzle = sw[0] == sw[1] && sw[1] == sw[2] && sw[2] == sw[3];

        if (!singleSwizzle)
        {
            // Fix when RSQ reads from unitialized components
            if (pIntermediate->InstructionType == IMD_ILU && pIntermediate->ILU == ILU_RSQ) {
                int swizzle = (pIntermediate->Output.Mask[0]) | (pIntermediate->Output.Mask[1] << 1) | (pIntermediate->Output.Mask[2] << 2) | (pIntermediate->Output.Mask[3] << 3);
                switch (swizzle)
                {
                case 1:
                    VshSetSwizzle(&pIntermediate->Parameters[0], SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X);
                    break;
                case 2:
                    VshSetSwizzle(&pIntermediate->Parameters[0], SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y);
                    break;
                case 4:
                    VshSetSwizzle(&pIntermediate->Parameters[0], SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z);
                    break;
                case 8:
                    VshSetSwizzle(&pIntermediate->Parameters[0], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
                    break;
                case 15:
                default:
                    LOG_TEST_CASE("rsq instruction with invalid swizzle");
                    break;
                }
            }
        }

		for (int j = 0; j < 3; j++)
		{
			//if(pIntermediate->Parameters[j].Active)
			{
				if (pIntermediate->Parameters[j].Parameter.ParameterType == PARAM_R)
				{
					RUsage[pIntermediate->Parameters[j].Parameter.Address] = TRUE;
				}
				// Make constant registers range from 0 to 191 instead of -96 to 95
				if (pIntermediate->Parameters[j].Parameter.ParameterType == PARAM_C)
				{
					//if(pIntermediate->Parameters[j].Parameter.Address < 0)
					pIntermediate->Parameters[j].Parameter.Address += X_D3DVS_CONSTREG_BIAS;
				}

				if (pIntermediate->Parameters[j].Parameter.ParameterType == PARAM_V) {
					RegVIsUsedByShader[pIntermediate->Parameters[j].Parameter.Address] = TRUE;

					if (!RegVIsPresentInDeclaration[pIntermediate->Parameters[j].Parameter.Address]) {
						// This vertex register was not declared and therefore is not present within the Vertex Data object
						// We read from temporary registers instead, that are set based on constants, in-turn, set by SetVertexData4f
						// We count down from the highest available on the host because Xbox titles don't use values that high, and we read from c192 (one above maximum Xbox c191 constant) and up
						static int temporaryRegisterBase = g_D3DCaps.VS20Caps.NumTemps - 13;
						pIntermediate->Parameters[j].Parameter.ParameterType = PARAM_R;
						pIntermediate->Parameters[j].Parameter.Address += temporaryRegisterBase;
					} 
				}
			}
		}

        if(pIntermediate->Output.Type == IMD_OUTPUT_R)
        {
            RUsage[pIntermediate->Output.Address] = TRUE;
        }
        // Make constant registers range from 0 to 191 instead of -96 to 95
        if(pIntermediate->Output.Type == IMD_OUTPUT_C)
        {
			//if(pIntermediate->Output.Address < 0)
				pIntermediate->Output.Address += X_D3DVS_CONSTREG_BIAS;
        }



        if(pIntermediate->InstructionType == IMD_MAC && pIntermediate->MAC == MAC_DPH)
        {
			// 2010/01/12 - revel8n - attempt to alleviate conversion issues relate to the dph instruction

            // Replace dph with dp3 and add
            if(pIntermediate->Output.Type != IMD_OUTPUT_R)
            {
                // TODO: Complete dph support
                EmuLog(LOG_LEVEL::WARNING, "Can't simulate dph for other than output r registers (yet)");

				// attempt to find unused register...
				int outRegister = -1;
				for (int j = temporaryCount - 1; j >= 0; --j)
				{
                    if (j == 12) continue;

					if(!RUsage[j])
					{
						outRegister = j;
						break;
					}
				}

				// return failure if there are no available registers
				if (outRegister == -1)
				{
					return FALSE;
				}

				VSH_INTERMEDIATE_FORMAT TmpIntermediate = *pIntermediate;

				// modify the instructions
				// the register value is not needed beyond these instructions so setting the usage flag should not be necessary (??)
				pIntermediate->MAC = MAC_DP3;
				pIntermediate->Output.Type = IMD_OUTPUT_R;
				pIntermediate->Output.Address = outRegister;
				VshSetOutputMask(&pIntermediate->Output, TRUE, TRUE, TRUE, TRUE);

				TmpIntermediate.MAC = MAC_ADD;
				TmpIntermediate.Parameters[0].IndexesWithA0_X = FALSE;
				TmpIntermediate.Parameters[0].Parameter.ParameterType = PARAM_R;
				TmpIntermediate.Parameters[0].Parameter.Address = outRegister;
				TmpIntermediate.Parameters[0].Parameter.Neg = FALSE;
				VshSetSwizzle(&TmpIntermediate.Parameters[1], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
				// Is this output register a scalar
				if (TmpIntermediate.Output.Type == IMD_OUTPUT_O) {
					if ((TmpIntermediate.Output.Address == OREG_OFOG) || (TmpIntermediate.Output.Address == OREG_OPTS)) {
						// This fixes test case "Namco Museum 50th Anniversary"
						// The PC shader assembler doesn't like masks on scalar registers
						VshSetOutputMask(&TmpIntermediate.Output, TRUE, TRUE, TRUE, TRUE);
						// Make the first source parameter use the w swizzle too
						VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
					}
				}

				VshInsertIntermediate(pShader, &TmpIntermediate, i + 1);
            }
			else
			{
				VSH_INTERMEDIATE_FORMAT TmpIntermediate = *pIntermediate;
				pIntermediate->MAC = MAC_DP3;
				TmpIntermediate.MAC = MAC_ADD;
				TmpIntermediate.Parameters[0].IndexesWithA0_X = FALSE;
				TmpIntermediate.Parameters[0].Parameter.ParameterType = PARAM_R;
				TmpIntermediate.Parameters[0].Parameter.Address = TmpIntermediate.Output.Address;
				TmpIntermediate.Parameters[0].Parameter.Neg = FALSE;

				int swizzle = (TmpIntermediate.Output.Mask[0]) | (TmpIntermediate.Output.Mask[1] << 1) | (TmpIntermediate.Output.Mask[2] << 2) | (TmpIntermediate.Output.Mask[3] << 3);
				switch (swizzle)
				{
				case 1:
					VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X);
					break;
				case 2:
					VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y);
					break;
				case 4:
					VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z);
					break;
				case 8:
					VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
					break;
				case 15:
				default:
					VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W);
					break;
				}
				//VshSetSwizzle(&TmpIntermediate.Parameters[0], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
				VshSetSwizzle(&TmpIntermediate.Parameters[1], SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W);
				//VshSetOutputMask(&TmpIntermediate.Output, FALSE, FALSE, FALSE, TRUE);
				VshInsertIntermediate(pShader, &TmpIntermediate, i + 1);
			}
            i++;
        }
    }

    // Finally, iterate through the shader, replace all writes to oPos with writes to r12
    // This fixes shaders that read from r12 expecting to see oPos
    for (int i = 0; i < pShader->IntermediateCount; i++) {
        VSH_INTERMEDIATE_FORMAT* pIntermediate = &pShader->Intermediate[i];
        if (pIntermediate->Output.Type == IMD_OUTPUT_O && pIntermediate->Output.Address == OREG_OPOS) {
            pIntermediate->Output.Type = IMD_OUTPUT_R;
            pIntermediate->Output.Address = 12;
        }
    }

    // We append one additional instruction to mov oPos, r12
    VSH_INTERMEDIATE_FORMAT MovIntermediate = {0};
    MovIntermediate.MAC = MAC_MOV;
    MovIntermediate.Output.Type = IMD_OUTPUT_O;
    MovIntermediate.Output.Address = OREG_OPOS;
    MovIntermediate.Output.Mask[0] = true;
    MovIntermediate.Output.Mask[1] = true;
    MovIntermediate.Output.Mask[2] = true;
    MovIntermediate.Output.Mask[3] = true;
    MovIntermediate.Parameters[0].Active = true;
    MovIntermediate.Parameters[0].Parameter.ParameterType = PARAM_R;
    MovIntermediate.Parameters[0].Parameter.Address = 12;
    MovIntermediate.Parameters[0].Parameter.Swizzle[0] = SWIZZLE_X;
    MovIntermediate.Parameters[0].Parameter.Swizzle[1] = SWIZZLE_Y;
    MovIntermediate.Parameters[0].Parameter.Swizzle[2] = SWIZZLE_Z;
    MovIntermediate.Parameters[0].Parameter.Swizzle[3] = SWIZZLE_W;
    VshInsertIntermediate(pShader, &MovIntermediate, pShader->IntermediateCount);

    return TRUE;
}

// ****************************************************************************
// * Vertex shader declaration recompiler
// ****************************************************************************

class XboxVertexDeclarationConverter
{
protected:
	// Internal variables
	XTL::CxbxVertexShaderInfo* pVertexShaderInfoToSet;
	XTL::CxbxVertexShaderStreamInfo* pCurrentVertexShaderStreamInfo;
	DWORD temporaryCount;
	bool IsFixedFunction;
	XTL::D3DVERTEXELEMENT* pRecompiled;

public:
	// Output
	DWORD XboxDeclarationCount;
	DWORD HostDeclarationSize;

private:
	// VERTEX SHADER
	#define DEF_VSH_END 0xFFFFFFFF
	#define DEF_VSH_NOP 0x00000000

	static DWORD VshGetDeclarationCount(DWORD *pXboxDeclaration)
	{
		DWORD Pos = 0;
		while (pXboxDeclaration[Pos] != DEF_VSH_END)
		{
			Pos++;
		}

		return Pos + 1;
	}

	static inline DWORD VshGetTokenType(DWORD XboxToken)
	{
		return (XboxToken & X_D3DVSD_TOKENTYPEMASK) >> X_D3DVSD_TOKENTYPESHIFT;
	}

	static inline WORD VshGetVertexStream(DWORD XboxToken)
	{
		return (XboxToken & X_D3DVSD_STREAMNUMBERMASK) >> X_D3DVSD_STREAMNUMBERSHIFT;
	}

	inline DWORD VshGetVertexRegister(DWORD XboxToken)
	{
		DWORD regNum = (XboxToken & X_D3DVSD_VERTEXREGMASK) >> X_D3DVSD_VERTEXREGSHIFT;
		if (regNum >= temporaryCount /*12 for D3D8, D3D9 value depends on host GPU */) {
			// Lego Star Wars hits this
			LOG_TEST_CASE("RegNum > NumTemps");
		}
		return regNum;
	}

	inline DWORD VshGetVertexRegisterIn(DWORD XboxToken)
	{
		DWORD regNum = (XboxToken & X_D3DVSD_VERTEXREGINMASK) >> X_D3DVSD_VERTEXREGINSHIFT;
		if (regNum >= temporaryCount /*12 for D3D8, D3D9 value depends on host GPU */) {
			// Lego Star Wars hits this
			LOG_TEST_CASE("RegNum > NumTemps");
		}
		return regNum;
	}

	void VshConvertToken_NOP(DWORD *pXboxToken)
	{
		// D3DVSD_NOP
		if(*pXboxToken != DEF_VSH_NOP)
		{
			EmuLog(LOG_LEVEL::WARNING, "Token NOP found, but extra parameters are given!");
		}
		DbgVshPrintf("\tD3DVSD_NOP(),\n");
	}

	DWORD VshConvertToken_CONSTMEM(DWORD *pXboxToken)
	{
		using namespace XTL;

		// D3DVSD_CONST
		DbgVshPrintf("\tD3DVSD_CONST(");

		XTL::DWORD ConstantAddress = (*pXboxToken & X_D3DVSD_CONSTADDRESSMASK) >> X_D3DVSD_CONSTADDRESSSHIFT;
		XTL::DWORD Count           = (*pXboxToken & X_D3DVSD_CONSTCOUNTMASK) >> X_D3DVSD_CONSTCOUNTSHIFT;
		DbgVshPrintf("%d, %d),\n", ConstantAddress, Count);

		// TODO

		return Count * 4 + 1;
	}

	void VshConvertToken_TESSELATOR(DWORD *pXboxToken)
	{
		using namespace XTL;
		BYTE Index;

		if(*pXboxToken & X_D3DVSD_MASK_TESSUV)
		{
			XTL::DWORD VertexRegister    = VshGetVertexRegister(*pXboxToken);
			XTL::DWORD NewVertexRegister = VertexRegister;

			DbgVshPrintf("\tD3DVSD_TESSUV(");
			NewVertexRegister = Xb2PCRegisterType(VertexRegister, Index);
			DbgVshPrintf("),\n");

			// TODO : Expand on the setting of this TESSUV register element :
			pRecompiled->Method = D3DDECLMETHOD_UV; // TODO : Is this correct?
			pRecompiled->Usage = D3DDECLUSAGE(NewVertexRegister);
			pRecompiled->UsageIndex = Index;
		}
		else // D3DVSD_TESSNORMAL
		{
			XTL::DWORD VertexRegisterIn  = VshGetVertexRegisterIn(*pXboxToken);
			XTL::DWORD VertexRegisterOut = VshGetVertexRegister(*pXboxToken);

			XTL::DWORD NewVertexRegisterIn  = VertexRegisterIn;
			XTL::DWORD NewVertexRegisterOut = VertexRegisterOut;

			DbgVshPrintf("\tD3DVSD_TESSNORMAL(");
			NewVertexRegisterIn = Xb2PCRegisterType(VertexRegisterIn, Index);
			// TODO : Expand on the setting of this TESSNORMAL input register element :
			pRecompiled->Method = 0; // TODO ?
			pRecompiled->Usage = D3DDECLUSAGE(NewVertexRegisterIn);
			pRecompiled->UsageIndex = Index;
			DbgVshPrintf(", ");

			NewVertexRegisterOut = Xb2PCRegisterType(VertexRegisterOut, Index);
			// TODO : Expand on the setting of this TESSNORMAL output register element :
			pRecompiled++;
			pRecompiled->Method = D3DDECLMETHOD_CROSSUV; // TODO : Is this correct?
			pRecompiled->Usage = D3DDECLUSAGE(NewVertexRegisterOut);
			pRecompiled->UsageIndex = Index;
			DbgVshPrintf("),\n");
		}
	}

	void VshEndPreviousStreamPatch()
	{
		if(pCurrentVertexShaderStreamInfo) {
			DbgVshPrintf("\t// NeedPatching: %d\n", pCurrentVertexShaderStreamInfo->NeedPatch);
		}
	}

	void VshConvertToken_STREAM(DWORD *pXboxToken)
	{
		using namespace XTL;

		// D3DVSD_STREAM_TESS
		if(*pXboxToken & X_D3DVSD_STREAMTESSMASK)
		{
			DbgVshPrintf("\tD3DVSD_STREAM_TESS(),\n");
		}
		else // D3DVSD_STREAM
		{
			VshEndPreviousStreamPatch();

			XTL::DWORD StreamNumber = VshGetVertexStream(*pXboxToken);

			// new stream
			pCurrentVertexShaderStreamInfo = &(pVertexShaderInfoToSet->VertexStreams[StreamNumber]);
			pCurrentVertexShaderStreamInfo->NeedPatch = FALSE;
			pCurrentVertexShaderStreamInfo->DeclPosition = FALSE;
			pCurrentVertexShaderStreamInfo->CurrentStreamNumber = 0;
			pCurrentVertexShaderStreamInfo->HostVertexStride = 0;
			pCurrentVertexShaderStreamInfo->NumberOfVertexElements = 0;

			// Dxbx note : Use Dophin(s), FieldRender, MatrixPaletteSkinning and PersistDisplay as a testcase

			DbgVshPrintf("\tD3DVSD_STREAM(%u),\n", StreamNumber);

			pCurrentVertexShaderStreamInfo->CurrentStreamNumber = VshGetVertexStream(*pXboxToken);
			pVertexShaderInfoToSet->NumberOfVertexStreams++;
			// TODO : Keep a bitmask for all StreamNumber's seen?
		}
	}

	void VshConvertToken_STREAMDATA_SKIP(DWORD *pXboxToken)
	{
		using namespace XTL;

		XTL::WORD SkipCount = (*pXboxToken & X_D3DVSD_SKIPCOUNTMASK) >> X_D3DVSD_SKIPCOUNTSHIFT;
		DbgVshPrintf("\tD3DVSD_SKIP(%d),\n", SkipCount);
		pCurrentVertexShaderStreamInfo->HostVertexStride += (SkipCount * sizeof(DWORD));
	}

	void VshConvertToken_STREAMDATA_SKIPBYTES(DWORD* pXboxToken)
	{
		using namespace XTL;

		XTL::WORD SkipBytesCount = (*pXboxToken & X_D3DVSD_SKIPCOUNTMASK) >> X_D3DVSD_SKIPCOUNTSHIFT;

		DbgVshPrintf("\tD3DVSD_SKIPBYTES(%d), /* xbox ext. */\n", SkipBytesCount);
		if (SkipBytesCount % sizeof(XTL::DWORD)) {
			EmuLog(LOG_LEVEL::WARNING, "D3DVSD_SKIPBYTES can't be converted to D3DVSD_SKIP, not divisble by 4.");
		}

		pCurrentVertexShaderStreamInfo->HostVertexStride += SkipBytesCount;
	}

	void VshConvertToken_STREAMDATA_REG(DWORD *pXboxToken)
	{
		using namespace XTL;

		XTL::DWORD VertexRegister = VshGetVertexRegister(*pXboxToken);
		XTL::BOOL NeedPatching = FALSE;
		XTL::BYTE Index;

		BYTE HostVertexRegisterType;
		// If this is a fixed-function shader, use Xb2PCRegisterType
		DbgVshPrintf("\t\tD3DVSD_REG(");
		if (IsFixedFunction) {
			HostVertexRegisterType = Xb2PCRegisterType(VertexRegister, Index);
		} else {
			// D3DDECLUSAGE_TEXCOORD can be useds for any user-defined data
			// We need this because there is no reliable way to detect the real usage
			// Xbox has no concept of 'usage types', it only requires a list of attribute register numbers.
			// So we treat them all as 'user-defined' with an Index of the Vertex Register Index
			// this prevents information loss in shaders due to non-matching dcl types!
			HostVertexRegisterType = D3DDECLUSAGE_TEXCOORD;
			Index = (BYTE)VertexRegister;
			DbgVshPrintf("%d", Index);
		}
		DbgVshPrintf(", ");

		// Add this register to the list of declared registers
		RegVIsPresentInDeclaration[VertexRegister] = true;

		XTL::DWORD XboxVertexElementDataType = (*pXboxToken & X_D3DVSD_DATATYPEMASK) >> X_D3DVSD_DATATYPESHIFT;
		XTL::BYTE HostVertexElementDataType = 0;
		XTL::WORD HostVertexElementByteSize = 0;

		switch (XboxVertexElementDataType)
		{
		case X_D3DVSDT_FLOAT1: // 0x12:
			DbgVshPrintf("D3DVSDT_FLOAT1");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT1;
			HostVertexElementByteSize = 1 * sizeof(FLOAT);
			break;
		case X_D3DVSDT_FLOAT2: // 0x22:
			DbgVshPrintf("D3DVSDT_FLOAT2");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT2;
			HostVertexElementByteSize = 2 * sizeof(FLOAT);
			break;
		case X_D3DVSDT_FLOAT3: // 0x32:
			DbgVshPrintf("D3DVSDT_FLOAT3");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT3;
			HostVertexElementByteSize = 3 * sizeof(FLOAT);
			break;
		case X_D3DVSDT_FLOAT4: // 0x42:
			DbgVshPrintf("D3DVSDT_FLOAT4");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT4;
			HostVertexElementByteSize = 4 * sizeof(FLOAT);
			break;
		case X_D3DVSDT_D3DCOLOR: // 0x40:
			DbgVshPrintf("D3DVSDT_D3DCOLOR");
			HostVertexElementDataType = D3DDECLTYPE_D3DCOLOR;
			HostVertexElementByteSize = 1 * sizeof(D3DCOLOR);
			break;
		case X_D3DVSDT_SHORT2: // 0x25:
			DbgVshPrintf("D3DVSDT_SHORT2");
			HostVertexElementDataType = D3DDECLTYPE_SHORT2;
			HostVertexElementByteSize = 2 * sizeof(XTL::SHORT);
			break;
		case X_D3DVSDT_SHORT4: // 0x45:
			DbgVshPrintf("D3DVSDT_SHORT4");
			HostVertexElementDataType = D3DDECLTYPE_SHORT4;
			HostVertexElementByteSize = 4 * sizeof(XTL::SHORT);
			break;
		case X_D3DVSDT_NORMSHORT1: // 0x11:
			DbgVshPrintf("D3DVSDT_NORMSHORT1 /* xbox ext. */");
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_SHORT2N) {
				HostVertexElementDataType = D3DDECLTYPE_SHORT2N;
				HostVertexElementByteSize = 2 * sizeof(SHORT);
			}
			else
			{
				HostVertexElementDataType = D3DDECLTYPE_FLOAT1;
				HostVertexElementByteSize = 1 * sizeof(FLOAT);
			}
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_NORMSHORT2: // 0x21:
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_SHORT2N) {
				DbgVshPrintf("D3DVSDT_NORMSHORT2");
				HostVertexElementDataType = D3DDECLTYPE_SHORT2N;
				HostVertexElementByteSize = 2 * sizeof(SHORT);
				// No need for patching in D3D9
			}
			else
			{
				DbgVshPrintf("D3DVSDT_NORMSHORT2 /* xbox ext. */");
				HostVertexElementDataType = D3DDECLTYPE_FLOAT2;
				HostVertexElementByteSize = 2 * sizeof(FLOAT);
				NeedPatching = TRUE;
			}
			break;
		case X_D3DVSDT_NORMSHORT3: // 0x31:
			DbgVshPrintf("D3DVSDT_NORMSHORT3 /* xbox ext. */");
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_SHORT4N) {
				HostVertexElementDataType = D3DDECLTYPE_SHORT4N;
				HostVertexElementByteSize = 4 * sizeof(SHORT);
			}
			else
			{
				HostVertexElementDataType = D3DDECLTYPE_FLOAT3;
				HostVertexElementByteSize = 3 * sizeof(FLOAT);
			}
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_NORMSHORT4: // 0x41:
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_SHORT4N) {
				DbgVshPrintf("D3DVSDT_NORMSHORT4");
				HostVertexElementDataType = D3DDECLTYPE_SHORT4N;
				HostVertexElementByteSize = 4 * sizeof(SHORT);
				// No need for patching in D3D9
			}
			else
			{
				DbgVshPrintf("D3DVSDT_NORMSHORT4 /* xbox ext. */");
				HostVertexElementDataType = D3DDECLTYPE_FLOAT4;
				HostVertexElementByteSize = 4 * sizeof(FLOAT);
				NeedPatching = TRUE;
			}
			break;
		case X_D3DVSDT_NORMPACKED3: // 0x16:
			DbgVshPrintf("D3DVSDT_NORMPACKED3 /* xbox ext. */");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT3;
			HostVertexElementByteSize = 3 * sizeof(FLOAT);
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_SHORT1: // 0x15:
			DbgVshPrintf("D3DVSDT_SHORT1 /* xbox ext. */");
			HostVertexElementDataType = D3DDECLTYPE_SHORT2;
			HostVertexElementByteSize = 2 * sizeof(XTL::SHORT);
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_SHORT3: // 0x35:
			DbgVshPrintf("D3DVSDT_SHORT3 /* xbox ext. */");
			HostVertexElementDataType = D3DDECLTYPE_SHORT4;
			HostVertexElementByteSize = 4 * sizeof(XTL::SHORT);
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_PBYTE1: // 0x14:
			DbgVshPrintf("D3DVSDT_PBYTE1 /* xbox ext. */");
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_UBYTE4N) {
				HostVertexElementDataType = D3DDECLTYPE_UBYTE4N;
				HostVertexElementByteSize = 4 * sizeof(BYTE);
			}
			else
			{
				HostVertexElementDataType = D3DDECLTYPE_FLOAT1;
				HostVertexElementByteSize = 1 * sizeof(FLOAT);
			}
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_PBYTE2: // 0x24:
			DbgVshPrintf("D3DVSDT_PBYTE2 /* xbox ext. */");
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_UBYTE4N) {
				HostVertexElementDataType = D3DDECLTYPE_UBYTE4N;
				HostVertexElementByteSize = 4 * sizeof(BYTE);
			}
			else
			{
				HostVertexElementDataType = D3DDECLTYPE_FLOAT2;
				HostVertexElementByteSize = 2 * sizeof(FLOAT);
			}
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_PBYTE3: // 0x34:
			DbgVshPrintf("D3DVSDT_PBYTE3 /* xbox ext. */");
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_UBYTE4N) {
				HostVertexElementDataType = D3DDECLTYPE_UBYTE4N;
				HostVertexElementByteSize = 4 * sizeof(BYTE);
			}
			else
			{
				HostVertexElementDataType = D3DDECLTYPE_FLOAT3;
				HostVertexElementByteSize = 3 * sizeof(FLOAT);
			}
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_PBYTE4: // 0x44:
			// Test-case : Panzer
			if (g_D3DCaps.DeclTypes & D3DDTCAPS_UBYTE4N) {
				DbgVshPrintf("D3DVSDT_PBYTE4");
				HostVertexElementDataType = D3DDECLTYPE_UBYTE4N;
				HostVertexElementByteSize = 4 * sizeof(BYTE);
				// No need for patching when D3D9 supports D3DDECLTYPE_UBYTE4N
			}
			else
			{
				DbgVshPrintf("D3DVSDT_PBYTE4 /* xbox ext. */");
				HostVertexElementDataType = D3DDECLTYPE_FLOAT4;
				HostVertexElementByteSize = 4 * sizeof(FLOAT);
				NeedPatching = TRUE;
			}
			break;
		case X_D3DVSDT_FLOAT2H: // 0x72:
			DbgVshPrintf("D3DVSDT_FLOAT2H /* xbox ext. */");
			HostVertexElementDataType = D3DDECLTYPE_FLOAT4;
			HostVertexElementByteSize = 4 * sizeof(FLOAT);
			NeedPatching = TRUE;
			break;
		case X_D3DVSDT_NONE: // 0x02:
			DbgVshPrintf("D3DVSDT_NONE /* xbox ext. */");
			// Ignore token
			break;
		default:
			DbgVshPrintf("Unknown data type for D3DVSD_REG: 0x%02X\n", XboxVertexElementDataType);
			break;
		}

		DbgVshPrintf("),\n");

		// On X_D3DVSDT_NONE skip this token
		if (XboxVertexElementDataType == X_D3DVSDT_NONE)
		{
			return;
		}

		// save patching information
		XTL::CxbxVertexShaderStreamElement *pCurrentElement = &(pCurrentVertexShaderStreamInfo->VertexElements[pCurrentVertexShaderStreamInfo->NumberOfVertexElements]);
		pCurrentElement->XboxType = XboxVertexElementDataType;
		pCurrentElement->HostByteSize = HostVertexElementByteSize;
		pCurrentVertexShaderStreamInfo->NumberOfVertexElements++;
		pCurrentVertexShaderStreamInfo->NeedPatch |= NeedPatching;

		pRecompiled->Stream = pCurrentVertexShaderStreamInfo->CurrentStreamNumber;
		pRecompiled->Offset = pCurrentVertexShaderStreamInfo->HostVertexStride;
		pRecompiled->Type = HostVertexElementDataType;
		pRecompiled->Method = D3DDECLMETHOD_DEFAULT;
		pRecompiled->Usage = HostVertexRegisterType;
		pRecompiled->UsageIndex = Index;

		pRecompiled++;

		pCurrentVertexShaderStreamInfo->HostVertexStride += HostVertexElementByteSize;

	}

	void VshConvertToken_STREAMDATA(DWORD *pXboxToken)
	{
		using namespace XTL;
		if (*pXboxToken & X_D3DVSD_MASK_SKIP)
		{
			// For D3D9, use D3DDECLTYPE_UNUSED ?
			if (*pXboxToken & X_D3DVSD_MASK_SKIPBYTES) {
				VshConvertToken_STREAMDATA_SKIPBYTES(pXboxToken);
			} else {
				VshConvertToken_STREAMDATA_SKIP(pXboxToken);
			}
		}
		else // D3DVSD_REG
		{
			VshConvertToken_STREAMDATA_REG(pXboxToken);
		}
	}

	DWORD VshRecompileToken(DWORD *pXboxToken)
	{
		using namespace XTL;

		XTL::DWORD Step = 1;

		switch(VshGetTokenType(*pXboxToken))
		{
		case X_D3DVSD_TOKEN_NOP:
			VshConvertToken_NOP(pXboxToken);
			break;
		case X_D3DVSD_TOKEN_STREAM:
		{
			VshConvertToken_STREAM(pXboxToken);
			break;
		}
		case X_D3DVSD_TOKEN_STREAMDATA:
		{
			VshConvertToken_STREAMDATA(pXboxToken);
			break;
		}
		case X_D3DVSD_TOKEN_TESSELLATOR:
		{
			VshConvertToken_TESSELATOR(pXboxToken);
			break;
		}
		case X_D3DVSD_TOKEN_CONSTMEM:
		{
			Step = VshConvertToken_CONSTMEM(pXboxToken);
			break;
		}
		default:
			DbgVshPrintf("Unknown token type: %d\n", VshGetTokenType(*pXboxToken));
			break;
		}

		return Step;
	}

public:
	XTL::D3DVERTEXELEMENT *Convert(DWORD* pXboxDeclaration, bool bIsFixedFunction, XTL::CxbxVertexShaderInfo* pCxbxVertexShaderInfo)
	{
		using namespace XTL;

		pVertexShaderInfoToSet = pCxbxVertexShaderInfo;
		temporaryCount = g_D3DCaps.VS20Caps.NumTemps;
		IsFixedFunction = bIsFixedFunction;

		RegVIsPresentInDeclaration.fill(false);

		// First of all some info:
		// We have to figure out which flags are set and then
		// we have to patch their params

		// some token values
		// 0xFFFFFFFF - end of the declaration
		// 0x00000000 - nop (means that this value is ignored)

		// Calculate size of declaration
		XboxDeclarationCount = VshGetDeclarationCount(pXboxDeclaration);
		// For Direct3D9, we need to reserve at least twice the number of elements, as one token can generate two registers (in and out) :
		HostDeclarationSize = XboxDeclarationCount * sizeof(D3DVERTEXELEMENT) * 2;
	
		D3DVERTEXELEMENT *Result = (D3DVERTEXELEMENT *)calloc(1, HostDeclarationSize);
		pRecompiled = Result;
		uint8_t *pRecompiledBufferOverflow = ((uint8_t*)pRecompiled) + HostDeclarationSize;

		DbgVshPrintf("DWORD dwVSHDecl[] =\n{\n");

		while (*pXboxDeclaration != DEF_VSH_END)
		{
			if ((uint8_t*)pRecompiled >= pRecompiledBufferOverflow) {
				DbgVshPrintf("Detected buffer-overflow, breaking out...\n");
				break;
			}

			DWORD Step = VshRecompileToken(pXboxDeclaration);
			pXboxDeclaration += Step;
		}

		*pRecompiled = D3DDECL_END();

		VshEndPreviousStreamPatch();
		DbgVshPrintf("\tD3DVSD_END()\n};\n");

		DbgVshPrintf("// NbrStreams: %d\n", pVertexShaderInfoToSet->NumberOfVertexStreams);

		return Result;
	}
};

XTL::D3DVERTEXELEMENT *XTL::EmuRecompileVshDeclaration
(
    DWORD                *pXboxDeclaration,
    bool                  bIsFixedFunction,
    DWORD                *pXboxDeclarationCount,
	DWORD                *pHostDeclarationSize,
    CxbxVertexShaderInfo *pCxbxVertexShaderInfo
)
{
	XboxVertexDeclarationConverter Converter;

	XTL::D3DVERTEXELEMENT* pHostVertexElements = Converter.Convert(pXboxDeclaration, bIsFixedFunction, pCxbxVertexShaderInfo);

	*pXboxDeclarationCount = Converter.XboxDeclarationCount;
	*pHostDeclarationSize = Converter.HostDeclarationSize;

    return pHostVertexElements;
}

// recompile xbox vertex shader function
extern HRESULT XTL::EmuRecompileVshFunction
(
    DWORD        *pXboxFunction,
    bool          bNoReservedConstants,
	D3DVERTEXELEMENT *pRecompiledDeclaration,
	bool   		 *pbUseDeclarationOnly,
    DWORD        *pXboxFunctionSize,
    LPD3DXBUFFER *ppRecompiledShader
)
{
	X_VSH_SHADER_HEADER   *pXboxVertexShaderHeader = (X_VSH_SHADER_HEADER*)pXboxFunction;
    DWORD               *pToken;
    boolean             EOI = false;
    VSH_XBOX_SHADER     *pShader = (VSH_XBOX_SHADER*)calloc(1, sizeof(VSH_XBOX_SHADER));
	LPD3DXBUFFER		pErrors = NULL;
    HRESULT             hRet = 0;

    // TODO: support this situation..
    if(pXboxFunction == NULL)
        return E_FAIL;

	// Initialize output arguments to zero
	*pbUseDeclarationOnly = 0;
    *pXboxFunctionSize = 0;
    *ppRecompiledShader = NULL;

    if(!pShader)
    {
        EmuLog(LOG_LEVEL::WARNING, "Couldn't allocate memory for vertex shader conversion buffer");
        return E_OUTOFMEMORY;
    }
    pShader->ShaderHeader = *pXboxVertexShaderHeader;
    switch(pXboxVertexShaderHeader->Version)
    {
        case VERSION_XVS:
            break;
        case VERSION_XVSS:
            EmuLog(LOG_LEVEL::WARNING, "Might not support vertex state shaders?");
            hRet = E_FAIL;
            break;
        case VERSION_XVSW:
            EmuLog(LOG_LEVEL::WARNING, "Might not support vertex read/write shaders?");
            hRet = E_FAIL;
            break;
        default:
            EmuLog(LOG_LEVEL::WARNING, "Unknown vertex shader version 0x%02X", pXboxVertexShaderHeader->Version);
            hRet = E_FAIL;
            break;
    }

    if(SUCCEEDED(hRet))
    {
		RegVIsUsedByShader.fill(false);

        for (pToken = (DWORD*)((uint8_t*)pXboxFunction + sizeof(X_VSH_SHADER_HEADER)); !EOI; pToken += X_VSH_INSTRUCTION_SIZE)
        {
            VSH_SHADER_INSTRUCTION Inst;

            VshParseInstruction((uint32_t*)pToken, &Inst);
            VshConvertToIntermediate(&Inst, pShader);
            EOI = (boolean)VshGetField((uint32_t*)pToken, FLD_FINAL);
        }

        // The size of the shader is
        *pXboxFunctionSize = (intptr_t)pToken - (intptr_t)pXboxFunction;

		// Do not attempt to compile empty shaders
		if (pShader->IntermediateCount == 0) {
            // This is a declaration only shader, so there is no function to recompile
            *pbUseDeclarationOnly = 1;
			return D3D_OK;
		}

		std::stringstream& pXboxShaderDisassembly = std::stringstream();
		std::stringstream& pHostShaderDisassembly = std::stringstream();

		DbgVshPrintf("-- Before conversion --\n");
        VshWriteShader(pShader, pXboxShaderDisassembly, pRecompiledDeclaration, FALSE);
		DbgVshPrintf("%s", pXboxShaderDisassembly.str().c_str());
		DbgVshPrintf("-----------------------\n");

        VshConvertShader(pShader, bNoReservedConstants);
        VshWriteShader(pShader, pHostShaderDisassembly, pRecompiledDeclaration, TRUE);

		DbgVshPrintf("-- After conversion ---\n");
		DbgVshPrintf("%s", pHostShaderDisassembly.str().c_str());
		DbgVshPrintf("-----------------------\n");

        // HACK: Azurik. Prevent Direct3D from trying to assemble this.
		if(pHostShaderDisassembly.str() == "vs.2.x\n")
		{
			EmuLog(LOG_LEVEL::WARNING, "Replacing empty vertex shader with fallback");

			static const char dummy[] =
				"vs.2.x\n"
				"dp4 oPos.x, v0, c96\n"
				"dp4 oPos.y, v0, c97\n"
				"dp4 oPos.z, v0, c98\n"
				"dp4 oPos.w, v0, c99\n";

			hRet = D3DXAssembleShader(
				dummy,
				strlen(dummy),
				/*pDefines=*/nullptr,
				/*pInclude=*/nullptr,
				/*Flags=*/0, // Was D3DXASM_SKIPVALIDATION,
				/*ppCompiledShader=*/ppRecompiledShader,
				/*ppCompilationErrors*/nullptr);
		}
		else
		{
			hRet = D3DXAssembleShader(
				pHostShaderDisassembly.str().c_str(),
				pHostShaderDisassembly.str().length(),
				/*pDefines=*/nullptr,
				/*pInclude=*/nullptr,
				/*Flags=*/0, // Was D3DXASM_SKIPVALIDATION,
				/*ppCompiledShader=*/ppRecompiledShader,
				/*ppCompilationErrors*/&pErrors);
		}

        if (FAILED(hRet))
        {
            EmuLog(LOG_LEVEL::WARNING, "Couldn't assemble recompiled vertex shader");
			EmuLog(LOG_LEVEL::WARNING, "%s", pErrors->GetBufferPointer());
        }

		if( pErrors )
			pErrors->Release();
    }

    free(pShader);

    return hRet;
}

extern void XTL::FreeVertexDynamicPatch(CxbxVertexShader *pVertexShader)
{
    pVertexShader->VertexShaderInfo.NumberOfVertexStreams = 0;
}

// Checks for failed vertex shaders, and shaders that would need patching
boolean VshHandleIsValidShader(DWORD XboxVertexShaderHandle)
{
#if 0
	//printf( "VS = 0x%.08X\n", XboxVertexShaderHandle );

    XTL::CxbxVertexShader *pCxbxVertexShader = XTL::GetCxbxVertexShader(XboxVertexShaderHandle);
    if (pCxbxVertexShader) {
        if (pCxbxVertexShader->XboxStatus != 0)
        {
            return FALSE;
        }
        /*
        for (uint32 i = 0; i < pCxbxVertexShader->VertexShaderInfo.NumberOfVertexStreams; i++)
        {
            if (pCxbxVertexShader->VertexShaderInfo.VertexStreams[i].NeedPatch)
            {
                // Just for caching purposes
                pCxbxVertexShader->XboxStatus = 0x80000001;
                return FALSE;
            }
        }
        */
    }
#endif
    return TRUE;
}

extern boolean XTL::IsValidCurrentShader(void)
{
	// Dxbx addition : There's no need to call
	// XTL_EmuIDirect3DDevice_GetVertexShader, just check g_Xbox_VertexShader_Handle :
	return VshHandleIsValidShader(g_Xbox_VertexShader_Handle);
}

XTL::CxbxVertexShaderInfo *GetCxbxVertexShaderInfo(DWORD XboxVertexShaderHandle)
{
    XTL::CxbxVertexShader *pCxbxVertexShader = XTL::GetCxbxVertexShader(XboxVertexShaderHandle);

    for (uint32_t i = 0; i < pCxbxVertexShader->VertexShaderInfo.NumberOfVertexStreams; i++)
    {
        if (pCxbxVertexShader->VertexShaderInfo.VertexStreams[i].NeedPatch)
        {
            return &pCxbxVertexShader->VertexShaderInfo;
        }
    }
    return NULL;
}

std::unordered_map<DWORD, XTL::CxbxVertexShader*> g_CxbxVertexShaders;

XTL::CxbxVertexShader* XTL::GetCxbxVertexShader(DWORD XboxVertexShaderHandle)
{
	if (VshHandleIsVertexShader(XboxVertexShaderHandle)) {
		auto it = g_CxbxVertexShaders.find(XboxVertexShaderHandle);
		if (it != g_CxbxVertexShaders.end()) {
			return it->second;
		}
	}

	return nullptr;
}

void XTL::SetCxbxVertexShader(DWORD XboxVertexShaderHandle, XTL::CxbxVertexShader* shader)
{
	auto it = g_CxbxVertexShaders.find(XboxVertexShaderHandle);
	if (it != g_CxbxVertexShaders.end() && it->second != nullptr && shader != nullptr) {
		LOG_TEST_CASE("Overwriting existing Vertex Shader");
	}

	g_CxbxVertexShaders[XboxVertexShaderHandle] = shader;
}

XTL::X_D3DVertexShader* CxbxGetVertexShader()
{
	using namespace XTL;

	X_D3DVertexShader* pXboxVertexShader = xbnullptr;

	// Only when we're sure of the location of the Xbox Device.m_pVertexShader variable
	if (g_XboxAddr_pVertexShader) {
		// read that (so that we get access to internal vertex shaders, like those generated
		// to contain the attribute-information for FVF shaders) :
		pXboxVertexShader = (X_D3DVertexShader*)(*g_XboxAddr_pVertexShader);
	}
	else
	{
		LOG_TEST_CASE("Unknown pVertexShader symbol location!");
		// Otherwise, we have no choice but to use what we've last stored in the
		// g_Xbox_VertexShader_Handle variable via our D3DDevice_SetVertexShader
		// and D3DDevice_SelectVertexShader* patches.

		// Note, that once we have a fail-safe way to determine the location of the
		// Xbox Device.m_pVertexShader symbol and the accompanying Address, we no longer need
		// this statement block and our patches on D3DDevice_SetVertexShader and
		// D3DDevice_SelectVertexShader* !

		// Now, to convert, we do need to have a valid vertex shader :
		if (g_Xbox_VertexShader_Handle == 0) {
			LOG_TEST_CASE("Unassigned Xbox vertex shader!");
			return nullptr;
		}

		if (!VshHandleIsVertexShader(g_Xbox_VertexShader_Handle)) {
			LOG_TEST_CASE("Xbox vertex shader lacks X_D3DFVF_RESERVED0 bit!");
			return nullptr;
		}

		pXboxVertexShader = VshHandleToXboxVertexShader(g_Xbox_VertexShader_Handle);
	}

	return pXboxVertexShader;
}

typedef struct {
	int XboxDeclarationFormat; // X_D3DVSDT_* key
	int XboxNrOfUnits;
	int XboxSizeInBytes;
	//int NrOfDimensions;
	XTL::D3DDECLTYPE HostDeclType;
	bool IsCompatible;
	int HostNrOfUnits;
	int HostSizeInBytes;
} XboxVertexAttributeDeclarationDecoded_t;

void CxbxDecodeVertexAttributeFormat(DWORD AttributeFormat, XboxVertexAttributeDeclarationDecoded_t* pDecoded)
{
	using namespace XTL;

	int NV2AType = (AttributeFormat & 0x0F) >> 0;
	int NV2ASize = (AttributeFormat & 0xF0) >> 4;

	assert(NV2AType <= 6 && NV2AType != 3); // only 0,1,2 and 4,5,6 are valid types (0:X_D3DVSDT_D3DCOLOR,1:X_D3DVSDT_NORMSHORT*,2:X_D3DVSDT_FLOAT* and 4:X_D3DVSDT_PBYTE*,5:X_D3DVSDT_SHORT*,6:X_D3DVSDT_NORMPACKED3)
	assert(NV2ASize <= 4 || NV2ASize == 7); // only 0..4 and 7 are valid sizes

	int XboxBytesPerUnit = 1 << (NV2AType & 3); // 0 and 4 use byte units, 1 and 5 use short units, 2 and 6 use float/dword units.
	int XboxNrOfUnits = (AttributeFormat == X_D3DVSDT_FLOAT2H) ? 3 : NV2ASize; // Identity-map sizes to units, except X_D3DVSDT_FLOAT2H becomes 3 units (the only one that has NV2ASize == 7)
	int XboxSizeInBytes = XboxNrOfUnits * XboxBytesPerUnit;
	//int NrOfDimensions = (AttributeFormat == X_D3DVSDT_NORMPACKED3) ? 3 : (AttributeFormat == X_D3DVSDT_FLOAT2H) ? 4 : XboxNrOfUnits;

#define X ((D3DDECLTYPE)-1) // Marks an invalid entry
	static const D3DDECLTYPE HostDeclTypeMap[7/*NV2AType*/][8/*NV2ASize*/] = {
		// 0x?0 : X_D3DVSDT_D3DCOLOR=0x40
		{ X, X, X, X, D3DDECLTYPE_D3DCOLOR, X, X, X },
		// 0x?1 : X_D3DVSDT_NORMSHORT1=0x11, X_D3DVSDT_NORMSHORT2=0x21, X_D3DVSDT_NORMSHORT3=0x31, X_D3DVSDT_NORMSHORT4=0x41
		{ X, D3DDECLTYPE_SHORT2N, D3DDECLTYPE_SHORT2N, D3DDECLTYPE_SHORT4N, D3DDECLTYPE_SHORT4N, X, X, X },
		// 0x?2 : X_D3DVSDT_NONE=0x02, X_D3DVSDT_FLOAT1=0x12, X_D3DVSDT_FLOAT2=0x22,X_D3DVSDT_FLOAT3=0x32,X_D3DVSDT_FLOAT4=0x42, X_D3DVSDT_FLOAT2H=0x72
		{ D3DDECLTYPE_UNUSED, D3DDECLTYPE_FLOAT1, D3DDECLTYPE_FLOAT2, D3DDECLTYPE_FLOAT3, D3DDECLTYPE_FLOAT4, X, X, D3DDECLTYPE_FLOAT4 },
		// 0x03 : unused
		{ X, X, X, X, X, X, X, X },
		// 0x?4 : X_D3DVSDT_PBYTE1=0x14, X_D3DVSDT_PBYTE2=0x24, X_D3DVSDT_PBYTE3=0x34, X_D3DVSDT_PBYTE4=0x44
		{ X, D3DDECLTYPE_UBYTE4N, D3DDECLTYPE_UBYTE4N, D3DDECLTYPE_UBYTE4N, D3DDECLTYPE_UBYTE4N, X, X, X },
		// 0x?5 : X_D3DVSDT_SHORT1=0x15, X_D3DVSDT_SHORT2=0x25, X_D3DVSDT_SHORT3=0x35, X_D3DVSDT_SHORT4=0x45
		{ X, D3DDECLTYPE_SHORT2, D3DDECLTYPE_SHORT2, D3DDECLTYPE_SHORT4, D3DDECLTYPE_SHORT4, X, X, X },
		// 0x?6 : X_D3DVSDT_NORMPACKED3 = 0x16
		{ X, D3DDECLTYPE_FLOAT3, X, X, X, X, X, X }
	};
#undef X

	// Lookup optimal host D3DDECLTYPE (might have a different number of units and/or unit size) :
	D3DDECLTYPE HostDeclType = HostDeclTypeMap[NV2AType][NV2ASize];

	// Definition of all known host D3DDECLTYPE enums (number of units, size of each unit and optional capability to check)
	static const struct {
		int NrOfUnits; int SizeOfUnit; DWORD D3DDTCAPS;
	} c_HostDeclTypeInfo[(int)D3DDECLTYPE_UNUSED + 1] =  {
		/*D3DDECLTYPE_FLOAT1     =  0 */ { 1, sizeof(float) },
		/*D3DDECLTYPE_FLOAT2     =  1 */ { 2, sizeof(float) },
		/*D3DDECLTYPE_FLOAT3     =  2 */ { 3, sizeof(float) },
		/*D3DDECLTYPE_FLOAT4     =  3 */ { 4, sizeof(float) },
		/*D3DDECLTYPE_D3DCOLOR   =  4 */ { 1, sizeof(D3DCOLOR) },
		/*D3DDECLTYPE_UBYTE4     =  5 */ { 4, sizeof(uint8_t), D3DDTCAPS_UBYTE4 },
		/*D3DDECLTYPE_SHORT2     =  6 */ { 2, sizeof(int16_t) },
		/*D3DDECLTYPE_SHORT4     =  7 */ { 4, sizeof(int16_t) },
		/*D3DDECLTYPE_UBYTE4N    =  8 */ { 4, sizeof(uint8_t), D3DDTCAPS_UBYTE4 },
		/*D3DDECLTYPE_SHORT2N    =  9 */ { 2, sizeof(int16_t), D3DDTCAPS_SHORT2N },
		/*D3DDECLTYPE_SHORT4N    = 10 */ { 4, sizeof(int16_t), D3DDTCAPS_SHORT4N },
		// For completeness, these are not used in HostDeclTypeMap :
		/*D3DDECLTYPE_USHORT2N   = 11 */ { 2, sizeof(int16_t), D3DDTCAPS_USHORT2N },
		/*D3DDECLTYPE_USHORT4N   = 12 */ { 4, sizeof(int16_t), D3DDTCAPS_USHORT4N },
		/*D3DDECLTYPE_UDEC3      = 13 */ { 1, sizeof(DWORD), D3DDTCAPS_UDEC3 },
		/*D3DDECLTYPE_DEC3N      = 14 */ { 1, sizeof(DWORD), D3DDTCAPS_DEC3N },
		/*D3DDECLTYPE_FLOAT16_2  = 15 */ { 2, sizeof(binary16_t), D3DDTCAPS_FLOAT16_2 },
		/*D3DDECLTYPE_FLOAT16_4  = 16 */ { 4, sizeof(binary16_t), D3DDTCAPS_FLOAT16_4 },
		/*D3DDECLTYPE_UNUSED     = 17 */ { 0 } // dynamic size
	};

	// Is there a capability flag to check?
	if (c_HostDeclTypeInfo[HostDeclType].D3DDTCAPS) {
		// and is it unsupported on this driver ?
		if ((g_D3DCaps.DeclTypes & c_HostDeclTypeInfo[HostDeclType].D3DDTCAPS) == 0) {
			// Then fallback to the required number of floats :
			HostDeclType = (D3DDECLTYPE)((int)D3DDECLTYPE_FLOAT1 + XboxNrOfUnits - 1);
		}
	}

	// Gather info on the host declaration (potentially a fallback), and check if it is Xbox compatible :
	int HostNrOfUnits = c_HostDeclTypeInfo[HostDeclType].NrOfUnits;
	int HostSizeInBytes = HostNrOfUnits * c_HostDeclTypeInfo[HostDeclType].SizeOfUnit;
	bool IsCompatible = (XboxNrOfUnits == HostNrOfUnits) && (XboxSizeInBytes == HostSizeInBytes);

	// Return the results :
	pDecoded->XboxDeclarationFormat = AttributeFormat; // Key during conversion
	pDecoded->XboxNrOfUnits = XboxNrOfUnits; // Needed for conversion
	pDecoded->XboxSizeInBytes = XboxSizeInBytes; // Amount of space taken in vertex
	//pDecoded->NrOfDimensions = NrOfDimensions; //
	pDecoded->HostDeclType = HostDeclType;
	pDecoded->IsCompatible = IsCompatible;
	pDecoded->HostNrOfUnits = HostNrOfUnits;
	pDecoded->HostSizeInBytes = HostSizeInBytes;
}

XTL::IDirect3DVertexBuffer* CxbxConvertXboxVertexBufferCompletely(XTL::X_D3DVertexBuffer* pXboxVertexBuffer)
{
	// TODO
	return nullptr;
}

XTL::IDirect3DVertexBuffer* CxbxConvertXboxVertexBufferSingleAttribute(XTL::X_D3DVertexBuffer* pXboxVertexBuffer, XboxVertexAttributeDeclarationDecoded_t* pDecodedAttribute, unsigned AttriubuteOffsetInStream)
{
	// TODO
	return nullptr;
}

// Converts Xbox vertex declaration towards a host vertex declaration,
// splitting up each attribute into a separate stream per host element,
// so that we can use an unmodified host clone of the Xbox vertex buffer
// for all compatible data types, and separate host streams for conversions.
void CxbxUpdateActiveVertexDeclaration(XTL::X_VERTEXATTRIBUTEFORMAT* pVertexAttributes, XTL::X_STREAMINPUT *pStreamInputs)
{
	using namespace XTL;

	assert(pXboxVertexShader);

	LOG_INIT;

	HRESULT hRet = D3D_OK;
	D3DVERTEXELEMENT HostVertexElements[X_VSH_NBR_ATTRIBUTES + 1] = { 0 };

	// Here, parse the Xbox declaration, converting it to a host declaration
	for (int AttributeIndex = 0; AttributeIndex < X_VSH_NBR_ATTRIBUTES; AttributeIndex++) {
		// Fetch the Xbox stream for this attribute :
		X_VERTEXSHADERINPUT *pAttributeSlot = &(pVertexAttributes->Slots[AttributeIndex]);
		// We'll call g_pD3DDevice->SetStreamSource for each attribute with these (initially empty) arguments :
		IDirect3DVertexBuffer *pHostVertexBuffer = nullptr;
		UINT StreamStride = 0;
		UINT StreamOffset = 0;
		// Does this attribute use no storage present the vertex (check this as early as possible to avoid needless processing) ?
		if (pAttributeSlot->Format == X_D3DVSDT_NONE)
		{
			// Handle tesselating attributes
			switch (pAttributeSlot->TesselationType) {
			case 0: break; // AUTONONE
			case 1: // AUTONORMAL
				// Note : .Stream, .Offset and .Type are copied from pAttributeSlot->TesselationSource in a post-processing step below,
				// because these could go through an Xbox to host conversion step, so must be copied over afterwards.
				HostVertexElements[AttributeIndex].Method = D3DDECLMETHOD_CROSSUV; // for D3DVSD_TESSNORMAL
				HostVertexElements[AttributeIndex].Usage = D3DDECLUSAGE_NORMAL; // TODO : Is this correct?
				HostVertexElements[AttributeIndex].UsageIndex = 1; // TODO : Is this correct?
				break;
			case 2: // AUTOTEXCOORD
				// HostVertexElements[AttributeIndex].Stream = 0; // The input stream is unused (but must be set to 0), which is already done above
				// HostVertexElements[AttributeIndex].Offset = 0; // The input offset is unused (but must be set to 0), which is already done above
				HostVertexElements[AttributeIndex].Type = D3DDECLTYPE_UNUSED; // The input type for D3DDECLMETHOD_UV must be D3DDECLTYPE_UNUSED (the output type implied by D3DDECLMETHOD_UV is D3DDECLTYPE_FLOAT2)
				HostVertexElements[AttributeIndex].Method = D3DDECLMETHOD_UV; // For X_D3DVSD_MASK_TESSUV
				HostVertexElements[AttributeIndex].Usage = D3DDECLUSAGE_NORMAL; // Note : In Fixed Function Vertex Pipeline, D3DDECLMETHOD_UV must specify usage D3DDECLUSAGE_TEXCOORD or D3DDECLUSAGE_BLENDWEIGHT. TODO : So, what to do?
				HostVertexElements[AttributeIndex].UsageIndex = 1; // TODO ; Is this correct?
				break;
			default:
				assert(false); // invalid TesselationType
			}
		}
		else
		{
			// Each attribute specifies the Xbox stream number it must be read from :
			unsigned XboxStreamIndex = pAttributeSlot->IndexOfStream;
			// When input streams are overriden, the stream indices referenced by attribute slots should stay in bounds!
			assert((g_Xbox_SetVertexShaderInput_Count == 0) || (XboxStreamIndex < g_Xbox_SetVertexShaderInput_Count));

			// Fetch the input stream from the pStreamInputs array (this argument honors overrides) :
			XTL::X_STREAMINPUT *pXboxStreamInput = &(pStreamInputs[XboxStreamIndex]);
			// Fetch the Xbox vertex buffer for this input stream :
			X_D3DVertexBuffer *pXboxVertexBuffer = pXboxStreamInput->VertexBuffer;
	// TODO : What if pXboxVertexBuffer is null? (This implies that the Xbox vertex declaration mentions a stream that's not yet been set using D3DDevice_SetVertexShaderInput or D3DDevice_SetStreamSource*
			// Decode the Xbox NV2A attribute format, including mapping to host :
			XboxVertexAttributeDeclarationDecoded_t DecodedAttribute;
			CxbxDecodeVertexAttributeFormat(pAttributeSlot->Format, &DecodedAttribute);
			// All non-X_D3DVSDT_NONE formats should have valid data :
			assert(pXboxVertexBuffer);
			assert(DecodedAttribute.XboxSizeInBytes > 0);

			int HostAttributeOffsetInVertex;

			// Can we use the Xbox attribute as-is?
			if (DecodedAttribute.IsCompatible) {
				// All we need to do, is use the host counterpart with unmodified Xbox contents :
				pHostVertexBuffer = CxbxConvertXboxVertexBufferCompletely(pXboxVertexBuffer);
				StreamStride = pXboxStreamInput->Stride;
				StreamOffset = pXboxStreamInput->Offset; // Can only become non-zero when g_Xbox_SetVertexShaderInput_Data is active
				HostAttributeOffsetInVertex = StreamOffset;
			}
			else
			{
				pHostVertexBuffer = CxbxConvertXboxVertexBufferSingleAttribute(pXboxVertexBuffer, &DecodedAttribute, pXboxStreamInput->Offset);
				// TODO : Should we map StreamOffset?
				assert(StreamStride == 0); // This dedicated VertexBuffer has no other contents, so it has no stride. TODO : Or must we set DecodedAttribute.HostSizeInBytes?
				HostAttributeOffsetInVertex = 0; // The dedicated stream contains only this attribute, so it's offset in the 'vertex' stream must be zero
			}

			static const struct {
				D3DDECLUSAGE Usage;
				BYTE UsageIndex = 0;
			} c_XboxAtrributeInfo[X_VSH_NBR_ATTRIBUTES] = { // TODO : Review this - perhaps everything
				/*[ 0]:*/{ D3DDECLUSAGE_POSITION }, // for X_D3DVSDE_POSITION -- TODO : Use D3DDECLUSAGE_POSITIONT instead?
				/*[ 1]:*/{ D3DDECLUSAGE_BLENDWEIGHT }, // for X_D3DVSDE_BLENDWEIGHT
				/*[ 2]:*/{ D3DDECLUSAGE_NORMAL, 0 } , // for X_D3DVSDE_NORMAL -- TODO : Is this correct?
				/*[ 3]:*/{ D3DDECLUSAGE_COLOR, 0 } , // for X_D3DVSDE_DIFFUSE
				/*[ 4]:*/{ D3DDECLUSAGE_COLOR, 1 } , // for X_D3DVSDE_SPECULAR
				/*[ 5]:*/{ D3DDECLUSAGE_FOG } , // for X_D3DVSDE_FOG
				/*[ 6]:*/{ D3DDECLUSAGE_PSIZE } , // for X_D3DVSDE_POINTSIZE
				/*[ 7]:*/{ D3DDECLUSAGE_COLOR, 2 } , // for X_D3DVSDE_BACKDIFFUSE
				/*[ 8]:*/{ D3DDECLUSAGE_COLOR, 3 } , // for X_D3DVSDE_BACKSPECULAR
				/*[ 9]:*/{ D3DDECLUSAGE_TEXCOORD, 0 } , // for X_D3DVSDE_TEXCOORD0
				/*[10]:*/{ D3DDECLUSAGE_TEXCOORD, 1 } , // for X_D3DVSDE_TEXCOORD1
				/*[11]:*/{ D3DDECLUSAGE_TEXCOORD, 2 } , // for X_D3DVSDE_TEXCOORD2
				/*[12]:*/{ D3DDECLUSAGE_TEXCOORD, 3 } , // for X_D3DVSDE_TEXCOORD3
				/*[13]:*/{ D3DDECLUSAGE_TEXCOORD, 4 } , // TODO : Is this a generic attribute?
				/*[14]:*/{ D3DDECLUSAGE_TEXCOORD, 5 } , // TODO : Is this a generic attribute?
				/*[15]:*/{ D3DDECLUSAGE_TEXCOORD, 6 } , // TODO : Is this a generic attribute?
			};

			HostVertexElements[AttributeIndex].Stream = AttributeIndex; // Stream index matches attribute index (each one has it's own stream)
			HostVertexElements[AttributeIndex].Offset = HostAttributeOffsetInVertex; // TODO : Or offset in *the stream* in bytes?
			HostVertexElements[AttributeIndex].Type = DecodedAttribute.HostDeclType; // Host vertex data type
			HostVertexElements[AttributeIndex].Method = D3DDECLMETHOD_DEFAULT; // Processing method. The input type for D3DDECLMETHOD_DEFAULT can be anything. The output type is the same as the input type.
			HostVertexElements[AttributeIndex].Usage = c_XboxAtrributeInfo[AttributeIndex].Usage; // Attribute semantics
			HostVertexElements[AttributeIndex].UsageIndex = c_XboxAtrributeInfo[AttributeIndex].UsageIndex; // Attribute semantic index
		}

		// Give each attribute (vertex element in host terms) it's own vertex stream :
		hRet = g_pD3DDevice->SetStreamSource(AttributeIndex, pHostVertexBuffer, StreamOffset + pAttributeSlot->Offset, StreamStride);
		//DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetStreamSource");
	}

	// Post-process host vertex elements that have a D3DDECLMETHOD_CROSSUV method :
	for (int AttributeIndex = 0; AttributeIndex < X_VSH_NBR_ATTRIBUTES; AttributeIndex++) {
		if (HostVertexElements[AttributeIndex].Method == D3DDECLMETHOD_CROSSUV)
		{
			int TesselationSource = pVertexAttributes->Slots[AttributeIndex].TesselationSource;
			// Copy over the Stream, Offset and Type of the host vertex element that serves as 'TesselationSource' :
			HostVertexElements[AttributeIndex].Stream = HostVertexElements[TesselationSource].Stream;
			HostVertexElements[AttributeIndex].Offset = HostVertexElements[TesselationSource].Offset;
			HostVertexElements[AttributeIndex].Type = HostVertexElements[TesselationSource].Type;
			// Note, the input type for D3DDECLMETHOD_CROSSUV can be D3DDECLTYPE_FLOAT[43], D3DDECLTYPE_D3DCOLOR, D3DDECLTYPE_UBYTE4, or D3DDECLTYPE_SHORT4
			// (the output type implied by D3DDECLMETHOD_CROSSUV is D3DDECLTYPE_FLOAT3).
			// TODO : Should we assert this?
		}
	}

	// TODO : Host doesn't seem to allow gaps, nor is D3DDECLTYPE_UNUSED allowed (apart from D3DDECLMETHOD_UV),
	// so it seems we need to condense the declarations here afterwards after all (and hope that the elements'
	// Usage+UsageIndex are correctly connected to the corresponding vertex shader registers) !?!

	// Mark the end of the vertex element declaration array :
	HostVertexElements[X_VSH_NBR_ATTRIBUTES] = D3DDECL_END();
	// Set the vertex declaration we've prepare above :
	IDirect3DVertexDeclaration *pHostVertexDeclaration = nullptr;
	hRet = g_pD3DDevice->CreateVertexDeclaration(HostVertexElements, &pHostVertexDeclaration);
	//DEBUG_D3DRESULT(hRet, "g_pD3DDevice->CreateVertexDeclaration"); // TODO : Why does this fail?!?
	hRet = g_pD3DDevice->SetVertexDeclaration(pHostVertexDeclaration);
	//DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexDeclaration");
	// Now that the declaration is set (or not, when conversion failed?)
	if (pHostVertexDeclaration)
	{
		// Throw away the temporary object reference already (host still holds a reference anyway):
		hRet = pHostVertexDeclaration->Release();
		//DEBUG_D3DRESULT(hRet, "pHostVertexDeclaration->Release");
	}
}

XTL::IDirect3DVertexShader *CxbxRecompileVertexProgram(DWORD *NV2AVertexProgram, unsigned ProgramSize)
{
	using namespace XTL;

	IDirect3DVertexShader *pHostVertexShader = nullptr;

	// TODO : Implement (including cache)

	return pHostVertexShader;
}


// Parse the Xbox Vertex shader program, convert and activate it, and transfer the constants present therein to host.
void CxbxParseAndConvertVertexShaderFunctionSlots()
{
	using namespace XTL;

	LOG_INIT;

	assert(g_Xbox_VertexShader_FunctionSlots_StartAddress < VSH_XBOX_MAX_INSTRUCTION_COUNT);

	HRESULT hRet;

	DWORD NV2AVertexProgram[VSH_XBOX_MAX_INSTRUCTION_COUNT * 4]; // This receives the actual program, without NV097_SET_TRANSFORM_PROGRAM interleaves
	unsigned CurrentProgramIndex = 0;
	UINT ConstantStartRegister = 0;

	DWORD* ProgramData = &(g_Xbox_VertexShader_FunctionSlots[g_Xbox_VertexShader_FunctionSlots_StartAddress * 4]);
	while (*ProgramData != 0) // Each program ends with a zero DWORD
	{
		// Parse the limited set of Vertex program NV2A Push buffer commands :
		DWORD NV2ACommand = *ProgramData++;
		switch (PUSH_METHOD(NV2ACommand)) {
		case 0x00000B00: { // == NV097_SET_TRANSFORM_PROGRAM
			unsigned NumberOfInstructions = PUSH_COUNT(NV2ACommand); // Fetch the number of instructions
			unsigned NumberOfDWORDs = NumberOfInstructions * 4;
			// Copy a batch of instructions :
			memcpy(&(NV2AVertexProgram[CurrentProgramIndex]), ProgramData, NumberOfDWORDs * sizeof(DWORD));
			CurrentProgramIndex += NumberOfDWORDs;
			// Each instruction takes 4 DWORD, so skip that number of DWORD's for the next batch :
			ProgramData += NumberOfDWORDs;
			continue;
		}
		case 0x00001EA4: // == NV097_SET_TRANSFORM_CONSTANT_LOAD
			ConstantStartRegister = *ProgramData++; // Remember the starting constant register
			assert(ConstantStartRegister < X_D3DVS_CONSTREG_COUNT);
			continue;
		case 0x00000B80: { // == NV097_SET_TRANSFORM_CONSTANT
			UINT Vector4fCount = PUSH_COUNT(NV2ACommand); // Fetch the number of constants
			assert(Vector4fCount > 0 && Vector4fCount < 8); // There can be at most 8 constants per batch
			// Set this batch of constant data on host :
			hRet = g_pD3DDevice->SetVertexShaderConstantF(ConstantStartRegister, (float*)ProgramData, Vector4fCount);
			//DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexShaderConstantF");
			// Each constant takes 4 floats, so skip that number of DWORD's for the next batch :
			unsigned NumberOfDWORDs = Vector4fCount * 4;
			ProgramData += NumberOfDWORDs;
			continue;
		}
		default:
			assert(false); // Unexpected NV2AMethod in vertex program!
		}

		break; // 
	}

	if (CurrentProgramIndex > 0)
	{
		// Recompile NV2AVertexProgram and set the result on host via SetVertexShader :
		IDirect3DVertexShader *pHostVertexShader = CxbxRecompileVertexProgram(NV2AVertexProgram, CurrentProgramIndex);
		if (pHostVertexShader)
		{
			hRet = g_pD3DDevice->SetVertexShader(pHostVertexShader);
			//DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexShader()");
			pHostVertexShader->Release();
			//DEBUG_D3DRESULT(hRet, "pHostVertexShader->Release()");
		}
	}
	else; // what?
}


void CxbxUpdateActiveVertexShader()
{
	using namespace XTL;

	assert(g_pD3DDevice);

	X_D3DVertexShader* pXboxVertexShader = CxbxGetVertexShader();
	if (pXboxVertexShader == xbnullptr)
	{
		LOG_TEST_CASE("Xbox should always have a VertexShader set (even for FVF's)");
		return;
	}

	// Take overrides (on declarations and streaminputs, as set by SetVertexShaderInput) into account :
	X_VERTEXATTRIBUTEFORMAT *pVertexAttributes = (g_Xbox_SetVertexShaderInput_Count > 0) ? &g_Xbox_SetVertexShaderInput_Attributes : &pXboxVertexShader->VertexAttribute;
	X_STREAMINPUT *pStreamInputs = (g_Xbox_SetVertexShaderInput_Count > 0) ? g_Xbox_SetVertexShaderInput_Data : g_SetStreamSources;

	CxbxUpdateActiveVertexDeclaration(pVertexAttributes, pStreamInputs);
	CxbxParseAndConvertVertexShaderFunctionSlots();
}

// Our vertex shader related EMU_PATCH implementations :

void CxbxImpl_LoadVertexShader(DWORD Handle, DWORD Address)
{
	using namespace XTL;

	if (!VshHandleIsVertexShader(Handle)) {
		LOG_TEST_CASE("D3DDevice_LoadVertexShader called without a shader!");
		return;
	}

	if (Address >= VSH_XBOX_MAX_INSTRUCTION_COUNT) {
		LOG_TEST_CASE("D3DDevice_LoadVertexShader would start too high!");
		return;
	}

	X_D3DVertexShader* XboxVertexShader = VshHandleToXboxVertexShader(Handle);
	if (Address + XboxVertexShader->TotalSize > VSH_XBOX_MAX_INSTRUCTION_COUNT) {
		LOG_TEST_CASE("D3DDevice_LoadVertexShader would exceed too far!");
		return;
	}

	// Copy the function data into the indicated address slots :
	for (unsigned i = 0; i < XboxVertexShader->TotalSize * 4; i++) { // Each slot is either 4 DWORDS or 4 floats
		g_Xbox_VertexShader_FunctionSlots[Address + i] = XboxVertexShader->FunctionData[i];
	}
}

void CxbxImpl_SelectVertexShader(DWORD Handle, DWORD Address)
{
	// Handle can be null if the current Xbox VertexShader is assigned
	if (Handle)
		// Handle can be an address of an Xbox VertexShader struct, or-ed with 1 (X_D3DFVF_RESERVED0)
		// If Handle is assigned, it becomes the new current Xbox VertexShader,
		// which resets a bit of state (nv2a execution mode, viewport, ?)
		g_Xbox_VertexShader_Handle = Handle;

	// Either way, the given address slot is selected as the start of the current vertex shader program
	// Address always indicates a previously loaded vertex shader slot (from where the program is used).
	g_Xbox_VertexShader_FunctionSlots_StartAddress = Address;
}

void CxbxImpl_SetVertexShader(DWORD Handle)
{
	g_Xbox_VertexShader_Handle = Handle;
	g_Xbox_VertexShader_FunctionSlots_StartAddress = 0;
}

void CxbxImpl_SetVertexShaderInput(DWORD Handle, UINT StreamCount, XTL::X_STREAMINPUT *pStreamInputs)
{
	using namespace XTL;

	if (Handle == NULL)
	{
		// Xbox doesn't remember a null-handle - this may be an XDK bug!
		// pStreamInputs is ingored
		g_Xbox_SetVertexShaderInput_Count = 0;
	}
	else
	{
		assert(VshHandleIsVertexShader(Handle));

		X_D3DVertexShader *pXboxVertexShader = VshHandleToXboxVertexShader(Handle);
		assert(pXboxVertexShader);

		// Xbox DOES store the Handle, but since it merely returns this through (unpatched) D3DDevice_GetVertexShaderInput, we don't have to.
		g_Xbox_SetVertexShaderInput_Count = StreamCount; // This indicates g_Xbox_SetVertexShaderInput_Data has to be used
		memcpy(g_Xbox_SetVertexShaderInput_Data, pStreamInputs, StreamCount * sizeof(XTL::X_STREAMINPUT)); // Make a copy of the supplied StreamInputs array
		g_Xbox_SetVertexShaderInput_Attributes = pXboxVertexShader->VertexAttribute; // Copy this vertex shaders's attributes
	}
}

void CxbxImpl_LoadVertexShaderProgram(DWORD* pFunction, DWORD Address)
{
	// The second word in the function indicates it's size, expressed in 4-DWORD-sized slots :
	DWORD NumberOfFourDWORDSlots = *pFunction++ >> 16;
	// Copy this data towards our storage for this, at the indicated address offset :
	DWORD SizeInBytes = NumberOfFourDWORDSlots * 4 * sizeof(DWORD);
	memcpy(&(g_Xbox_VertexShader_FunctionSlots[Address * 4]), pFunction, SizeInBytes);
}

// TODO : Replace this with an XREF in XbSymbolDatabase, since this is re-inventing the wheel:
void CxbxLocateVertexShaderSetter(DWORD Handle, DWORD Address)
{
	using namespace XTL;

	XB_trampoline(VOID, WINAPI, D3DDevice_SelectVertexShader, (DWORD, DWORD));
	XB_trampoline(VOID, __stdcall, D3DDevice_SelectVertexShader_0, ());
	XB_trampoline(VOID, __stdcall, D3DDevice_SelectVertexShader_4, (DWORD));
	XB_trampoline(VOID, WINAPI, D3DDevice_SetVertexShader, (DWORD));

	if (XB_D3DDevice_SelectVertexShader)
		XB_D3DDevice_SelectVertexShader(Handle, Address);
	else
		if (XB_D3DDevice_SelectVertexShader_0)
			__asm {
			mov eax, Address // TODO : Is this really needed?
			mov ebx, Handle // TODO : Is this really needed?
			call XB_D3DDevice_SelectVertexShader_0
		}
	if (XB_D3DDevice_SelectVertexShader_4)
		__asm {
		mov eax, Handle // TODO : Is this really needed?
		mov ebx, Address
		call XB_D3DDevice_SelectVertexShader_4
	}
	else
		// Pass through to the Xbox implementation of this function
		XB_D3DDevice_SetVertexShader(Handle);
}

// TODO : Replace this with an XREF in XbSymbolDatabase, since this is re-inventing the wheel:
// Scan the XBE memory for the location of the m_pVertexShader variable, by setting it indirectly
bool CxbxLocateVertexShader()
{
	const DWORD Handle1 = 0x5EAC0000;
	const DWORD Handle2 = 0xF14DEE80;
	DWORD Address = 135;
	DWORD HandleToRestore = g_Xbox_VertexShader_Handle;
	DWORD AddressToRestore = g_Xbox_VertexShader_FunctionSlots_StartAddress;

	// First, set an obscure value :
	CxbxLocateVertexShaderSetter(Handle1 | X_D3DFVF_RESERVED0, Address);
	// Scan over the entire XBE
	for (DWORD* SymbolLocation = (DWORD*)XBE_IMAGE_BASE; ; SymbolLocation++)
	{
		// If we see the obscure value (minus the X_D3DFVF_RESERVED0 flag)
		if (*SymbolLocation == Handle1) {
			// Set another value :
			CxbxLocateVertexShaderSetter(Handle2 | X_D3DFVF_RESERVED0, Address);
			// If the memory contents changed to the other value
			if (*SymbolLocation == Handle2) {
				// We got a hit!
				g_XboxAddr_pVertexShader = SymbolLocation;
				// Cleanup :
				CxbxLocateVertexShaderSetter(HandleToRestore, AddressToRestore);
				return true;
			}
		}
	}

	CxbxLocateVertexShaderSetter(HandleToRestore, AddressToRestore);
	return false;
}


#if 0 // Old code, remove once re-implemented

// Old Vertex shader program conversion code
{
	LPD3DXBUFFER  pRecompiledBuffer = NULL;
		DWORD         XboxFunctionSize = 0;
		DWORD        *pRecompiledFunction = NULL;
		if (SUCCEEDED(hRet) && pFunction)
		{
			bool bUseDeclarationOnly = false;

			hRet = XTL::EmuRecompileVshFunction((DWORD*)pFunction,
				/*bNoReservedConstants=*/g_VertexShaderConstantMode == X_D3DSCM_NORESERVEDCONSTANTS,
				pRecompiledDeclaration,
				&bUseDeclarationOnly,
				&XboxFunctionSize,
				&pRecompiledBuffer);
			if (SUCCEEDED(hRet))
			{
				if (!bUseDeclarationOnly)
					pRecompiledFunction = (DWORD*)pRecompiledBuffer->GetBufferPointer();
				else
					pRecompiledFunction = NULL;
			}
			else
			{
				pRecompiledFunction = NULL;
				EmuLog(LOG_LEVEL::WARNING, "Couldn't recompile vertex shader function.");
			}
		}

		//EmuLog(LOG_LEVEL::DEBUG, "MaxVertexShaderConst = %d", g_D3DCaps.MaxVertexShaderConst);

		IDirect3DVertexShader* pHostVertexShader = nullptr;
		if (SUCCEEDED(hRet) && pRecompiledFunction != nullptr)
		{
			hRet = g_pD3DDevice->CreateVertexShader
			(
				pRecompiledFunction,
				&pHostVertexShader
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->CreateVertexShader");
		}

		//* Fallback to dummy shader.
		if (FAILED(hRet))
		{
			static const char dummy[] =
				"vs.1.1\n"
				"dcl_position v0\n"
				"dp4 oPos.x, v0, c96\n"
				"dp4 oPos.y, v0, c97\n"
				"dp4 oPos.z, v0, c98\n"
				"dp4 oPos.w, v0, c99\n";

			EmuLog(LOG_LEVEL::WARNING, "Trying fallback:\n%s", dummy);

			hRet = D3DXAssembleShader(
				dummy,
				strlen(dummy),
				/*pDefines=*/nullptr,
				/*pInclude=*/nullptr,
				/*Flags=*/0, // Was D3DXASM_SKIPVALIDATION
				/*ppCompiledShader=*/&pRecompiledBuffer,
				/*ppCompilationErrors*/nullptr);

			DEBUG_D3DRESULT(hRet, "D3DXAssembleShader");

			hRet = g_pD3DDevice->CreateVertexShader
			(
				(DWORD*)pRecompiledBuffer->GetBufferPointer(),
				&pHostVertexShader
			);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->CreateVertexShader(fallback)");
		}

		if (pRecompiledBuffer != nullptr)
		{
			pRecompiledBuffer->Release();
			pRecompiledBuffer = nullptr;
		}

		free(pRecompiledDeclaration);

		pCxbxVertexShader->pXboxDeclarationCopy = (DWORD*)malloc(XboxDeclarationCount * sizeof(DWORD));
		memcpy(pCxbxVertexShader->pXboxDeclarationCopy, pDeclaration, XboxDeclarationCount * sizeof(DWORD));
		pCxbxVertexShader->XboxFunctionSize = 0;
		pCxbxVertexShader->pXboxFunctionCopy = NULL;
		pCxbxVertexShader->XboxVertexShaderType = X_VST_NORMAL; // TODO : This can vary
		pCxbxVertexShader->XboxNrAddressSlots = (XboxFunctionSize - sizeof(X_VSH_SHADER_HEADER)) / X_VSH_INSTRUCTION_SIZE_BYTES;
		pCxbxVertexShader->HostFVF = 0;
		pCxbxVertexShader->pHostVertexShader = nullptr;
		pCxbxVertexShader->XboxDeclarationCount = XboxDeclarationCount;
		pCxbxVertexShader->HostDeclarationSize = HostDeclarationSize;
		// Save the status, to remove things later
		// pCxbxVertexShader->XboxStatus = hRet; // Not even used by VshHandleIsValidShader()

		if (SUCCEEDED(hRet))
		{
			if (pFunction != NULL)
			{
				pCxbxVertexShader->XboxFunctionSize = XboxFunctionSize;
				pCxbxVertexShader->pXboxFunctionCopy = (DWORD*)malloc(XboxFunctionSize);
				memcpy(pCxbxVertexShader->pXboxFunctionCopy, pFunction, XboxFunctionSize);
			}

			pCxbxVertexShader->pHostVertexShader = pHostVertexShader;
		}
		else
		{
			LOG_TEST_CASE("Falling back to FVF shader");
			pCxbxVertexShader->HostFVF = D3DFVF_XYZ | D3DFVF_TEX0;
		}

		// Register the host Vertex Shader
		SetCxbxVertexShader(*pHandle, pCxbxVertexShader);

		if (FAILED(hRet))
		{
#ifdef _DEBUG_TRACK_VS
			if (pFunction)
			{
				char pFileName[30];
				static int FailedShaderCount = 0;
				X_VSH_SHADER_HEADER *pHeader = (X_VSH_SHADER_HEADER*)pFunction;
				EmuLog(LOG_LEVEL::WARNING, "Couldn't create vertex shader!");
				sprintf(pFileName, "failed%05d.xvu", FailedShaderCount);
				FILE *f = fopen(pFileName, "wb");
				if (f)
				{
					fwrite(pFunction, sizeof(X_VSH_SHADER_HEADER) + pHeader->NumInst * 16, 1, f);
					fclose(f);
				}
				FailedShaderCount++;
			}
#endif // _DEBUG_TRACK_VS
			//hRet = D3D_OK;
		}

		return hRet;
	}

	//void CxbxVertexShaderConvert()
	{
		using namespace XTL;

		LOG_INIT;

		HRESULT hRet = D3D_OK;
		DWORD Handle = g_Xbox_VertexShader_Handle;

		if (VshHandleIsVertexShader(Handle)) {
			CxbxVertexShader *pCxbxVertexShader = GetCxbxVertexShader(Handle);
			hRet = g_pD3DDevice->SetVertexDeclaration(pCxbxVertexShader->pHostVertexDeclaration);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexDeclaration");
			if (pCxbxVertexShader->HostFVF)
			{
				hRet = g_pD3DDevice->SetFVF(pCxbxVertexShader->HostFVF);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetFVF(VshHandleIsVertexShader)");
			}
			else
			{
				hRet = g_pD3DDevice->SetVertexShader(pCxbxVertexShader->pHostVertexShader);
				DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexShader(VshHandleIsVertexShader)");
			}

			// Set default constant values for specular, diffuse, etc
			static const float ColorBlack[4] = { 0,0,0,0 };
			static const float ColorWhite[4] = { 1,1,1,1 };

			g_pD3DDevice->SetVertexShaderConstantF(X_D3DVS_CONSTREG_VERTEXDATA4F_BASE + X_D3DVSDE_DIFFUSE, ColorWhite, 1);
			g_pD3DDevice->SetVertexShaderConstantF(X_D3DVS_CONSTREG_VERTEXDATA4F_BASE + X_D3DVSDE_BACKDIFFUSE, ColorWhite, 1);
			g_pD3DDevice->SetVertexShaderConstantF(X_D3DVS_CONSTREG_VERTEXDATA4F_BASE + X_D3DVSDE_SPECULAR, ColorBlack, 1);
			g_pD3DDevice->SetVertexShaderConstantF(X_D3DVS_CONSTREG_VERTEXDATA4F_BASE + X_D3DVSDE_BACKSPECULAR, ColorBlack, 1);
		}
		else {
			hRet = g_pD3DDevice->SetVertexShader(nullptr);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexShader");
			hRet = g_pD3DDevice->SetFVF(Handle);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetFVF");
		}

		UpdateViewPortOffsetAndScaleConstants();
	}

	// CxbxVertexShaderActivate()
	{
		using namespace XTL;

		LOG_INIT;

		DWORD Handle = g_Xbox_VertexShader_Handle; // TMP glue
		DWORD Address = g_Xbox_VertexShader_FunctionSlots_StartAddress; // TMP glue

		CxbxVertexShader* pCxbxVertexShader = nullptr;
		DWORD HostFVF = 0;

		if (VshHandleIsVertexShader(Handle))
		{
			pCxbxVertexShader = GetCxbxVertexShader(Handle);
		}
		else if (Handle == NULL)
		{
			HostFVF = D3DFVF_XYZ | D3DFVF_TEX0;
		}
		else if (Address < 136)
		{
			X_D3DVertexShader* pXboxVertexShader = (X_D3DVertexShader*)g_Xbox_VertexShader_FunctionSlots[Address * 4];

			if (pXboxVertexShader == nullptr)
			{
				EmuLog(LOG_LEVEL::WARNING, "g_Xbox_VertexShader_FunctionSlots[%d] = 0", Address);
			}
		}

		XTL::IDirect3DVertexDeclaration* pHostVertexDeclaration = nullptr;
		XTL::IDirect3DVertexShader* pHostVertexShader = nullptr;

		if (pCxbxVertexShader)
		{
			pHostVertexDeclaration = pCxbxVertexShader->pHostVertexDeclaration;
			pHostVertexShader = pCxbxVertexShader->pHostVertexShader;
			HostFVF = pCxbxVertexShader->HostFVF;
		}

		HRESULT hRet = g_pD3DDevice->SetVertexDeclaration(pHostVertexDeclaration);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexDeclaration()");
		hRet = g_pD3DDevice->SetVertexShader(pHostVertexShader);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetVertexShader()");
		if (HostFVF) // should equal (pHostVertexShader == nullptr)
		{
			hRet = g_pD3DDevice->SetFVF(HostFVF);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX0)");
		}

		if (FAILED(hRet))
		{
			EmuLog(LOG_LEVEL::WARNING, "We're lying about setting a vertext shader!");

			hRet = D3D_OK;
		}
	}
}

// old vertex shader program load code
{
	using namespace XTL;

	// Maps pFunction defintions to pre-compiled shaders
	// to reduce the speed impact of LoadVertexShaderProgram
	typedef uint64_t load_shader_program_key_t;
	static std::unordered_map<load_shader_program_key_t, DWORD> g_LoadVertexShaderProgramCache;

	load_shader_program_key_t shaderCacheKey = ((load_shader_program_key_t)g_Xbox_VertexShader_Handle << 32) | (DWORD)pFunction;

	// If the shader key was located in the cache, use the cached shader
	// TODO: When do we clear the cache? In this approach, shaders are
	// never freed...
	auto it = g_LoadVertexShaderProgramCache.find(shaderCacheKey);
	if (it != g_LoadVertexShaderProgramCache.end()) {
		EMUPATCH(D3DDevice_LoadVertexShader)(it->second, Address);
		EMUPATCH(D3DDevice_SelectVertexShader)(it->second, Address);
		return;
	}

	DWORD* pXboxVertexDeclaration = nullptr;

	if (VshHandleIsVertexShader(g_Xbox_VertexShader_Handle)) {
		CxbxVertexShader* pCxbxVertexShader = GetCxbxVertexShader(g_Xbox_VertexShader_Handle);

		// If we failed to fetch an active pixel shader, log and do nothing
		if (pCxbxVertexShader == nullptr) {
			LOG_TEST_CASE("D3DDevice_LoadVertexShaderProgram: Failed to locate original shader");
			return;
		}

		// Simply retrieve the contents of the existing vertex shader program
		pXboxVertexDeclaration = pCxbxVertexShader->pXboxDeclarationCopy;
	}
	else {
		// This is an unusual scenario in which an FVF-based shader is being replaced with an actual shader
		// But without calling CreateVertexShader: This means we need to parse the current FVF and generate
		// our own Xbox-like declaration to use when converting/setting this new shader

		// Define a large enough definition to contain all possible FVF types
		// 20 is maximum possible size
		XTL::DWORD CxbxXboxVertexDeclaration[20] = { 0 };
		int index = 0;

		// Write the Stream Number (always 0 for FVF)
		CxbxXboxVertexDeclaration[index++] = X_D3DVSD_STREAM(0);

		// Write Position
		DWORD position = (g_Xbox_VertexShader_Handle & X_D3DFVF_POSITION_MASK);
		if (position == X_D3DFVF_XYZRHW) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_POSITION, X_D3DVSDT_FLOAT4);
		}
		else {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_POSITION, X_D3DVSDT_FLOAT3);
		}

		// Write Blend Weights
		if (position == X_D3DFVF_XYZB1) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_BLENDWEIGHT, X_D3DVSDT_FLOAT1);
		}
		if (position == X_D3DFVF_XYZB2) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_BLENDWEIGHT, X_D3DVSDT_FLOAT2);
		}
		if (position == X_D3DFVF_XYZB3) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_BLENDWEIGHT, X_D3DVSDT_FLOAT3);
		}
		if (position == X_D3DFVF_XYZB4) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_BLENDWEIGHT, X_D3DVSDT_FLOAT4);
		}

		// Write Normal, Diffuse, and Specular
		if (g_Xbox_VertexShader_Handle & X_D3DFVF_NORMAL) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_NORMAL, X_D3DVSDT_FLOAT3);
		}
		if (g_Xbox_VertexShader_Handle & X_D3DFVF_DIFFUSE) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_DIFFUSE, X_D3DVSDT_D3DCOLOR);
		}
		if (g_Xbox_VertexShader_Handle & X_D3DFVF_SPECULAR) {
			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_SPECULAR, X_D3DVSDT_D3DCOLOR);
		}

		// Write Texture Coordinates
		int textureCount = (g_Xbox_VertexShader_Handle & X_D3DFVF_TEXCOUNT_MASK) >> X_D3DFVF_TEXCOUNT_SHIFT;
		assert(textureCount <= 4); // Safeguard, since the X_D3DFVF_TEXCOUNT bitfield could contain invalid values (5 up to 15)
		for (int i = 0; i < textureCount; i++) {
			int numberOfCoordinates = 0;

			if ((g_Xbox_VertexShader_Handle & X_D3DFVF_TEXCOORDSIZE1(i)) == (DWORD)X_D3DFVF_TEXCOORDSIZE1(i)) {
				numberOfCoordinates = X_D3DVSDT_FLOAT1;
			}
			if ((g_Xbox_VertexShader_Handle & X_D3DFVF_TEXCOORDSIZE2(i)) == (DWORD)X_D3DFVF_TEXCOORDSIZE2(i)) {
				numberOfCoordinates = X_D3DVSDT_FLOAT2;
			}
			if ((g_Xbox_VertexShader_Handle & X_D3DFVF_TEXCOORDSIZE3(i)) == (DWORD)X_D3DFVF_TEXCOORDSIZE3(i)) {
				numberOfCoordinates = X_D3DVSDT_FLOAT3;
			}
			if ((g_Xbox_VertexShader_Handle & X_D3DFVF_TEXCOORDSIZE4(i)) == (DWORD)X_D3DFVF_TEXCOORDSIZE4(i)) {
				numberOfCoordinates = X_D3DVSDT_FLOAT4;
			}

			CxbxXboxVertexDeclaration[index++] = X_D3DVSD_REG(X_D3DVSDE_TEXCOORD0 + i, numberOfCoordinates);
		}

		// Write Declaration End
		CxbxXboxVertexDeclaration[index++] = X_D3DVSD_END();

		pXboxVertexDeclaration = CxbxXboxVertexDeclaration;
		// Now we can fall through and create a new vertex shader
	}

	// Create a vertex shader with the new vertex program data
	DWORD hNewXboxShader = 0;
#if 0 // TODO : FIXME 
	HRESULT hr = EMUPATCH(D3DDevice_CreateVertexShader)(pXboxVertexDeclaration, pFunction, &hNewXboxShader, 0);

	if (FAILED(hr))
		CxbxKrnlCleanup("Error creating new vertex shader!");
#endif

	EMUPATCH(D3DDevice_LoadVertexShader)(hNewXboxShader, Address);
	EMUPATCH(D3DDevice_SelectVertexShader)(hNewXboxShader, Address);

	g_LoadVertexShaderProgramCache[shaderCacheKey] = hNewXboxShader;

	EmuLog(LOG_LEVEL::WARNING, "Vertex Shader Cache Size: %d", g_LoadVertexShaderProgramCache.size());
}

#endif
