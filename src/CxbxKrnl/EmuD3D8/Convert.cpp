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
// *   Cxbx->Win32->CxbxKrnl->EmuD3D8->Convert.cpp
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
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
#include "Convert.h"
#include "CxbxKrnl/EmuD3D8Logging.h"

// D3D build version
extern uint32 g_BuildVersion;

// About format color components:
// A = alpha, byte : 0 = fully opaque, 255 = fully transparent
// X = ignore these component bits
// R = red
// G = green
// B = blue
// L = luminance, byte : 0 = pure black ARGB(1, 0,0,0) to 255 = pure white ARGB(1,255,255,255)
// P = pallete
enum _ComponentEncoding {
	NoCmpnts = 0, // Format doesn't contain any component (ARGB/QWVU)
	A1R5G5B5,
	X1R5G5B5, // NOTE : A=255
	A4R4G4B4,
	__R5G6B5, // NOTE : A=255
	A8R8G8B8,
	X8R8G8B8, // NOTE : A=255
	____R8B8, // NOTE : A takes R, G takes B
	____G8B8, // NOTE : A takes G, R takes B
	______A8, // TEST : R=G=B= 255
	__R6G5B5, // NOTE : A=255
	R5G5B5A1,
	R4G4B4A4,
	A8B8G8R8,
	B8G8R8A8,
	R8G8B8A8,
	______L8, // NOTE : A=255, R=G=B= L
	_____AL8, // NOTE : A=R=G=B= L
	_____L16, // NOTE : Actually G8B8, with A=R=255
	____A8L8, // NOTE : R=G=B= L
	____DXT1,
	____DXT3,
	____DXT5,
	______P8,
};

// Conversion functions copied from libyuv
// See https://chromium.googlesource.com/libyuv/libyuv/+/master/source/row_common.cc
void RGB565ToARGBRow_C(const uint8* src_rgb565, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_rgb565[0] & 0x1f;
		uint8 g = (src_rgb565[0] >> 5) | ((src_rgb565[1] & 0x07) << 3);
		uint8 r = src_rgb565[1] >> 3;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 2) | (g >> 4);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_rgb565 += 2;
	}
}
void ARGB1555ToARGBRow_C(const uint8* src_argb1555,
	uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_argb1555[0] & 0x1f;
		uint8 g = (src_argb1555[0] >> 5) | ((src_argb1555[1] & 0x03) << 3);
		uint8 r = (src_argb1555[1] & 0x7c) >> 2;
		uint8 a = src_argb1555[1] >> 7;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = -a;
		dst_argb += 4;
		src_argb1555 += 2;
	}
}
void ARGB4444ToARGBRow_C(const uint8* src_argb4444,
	uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_argb4444[0] & 0x0f;
		uint8 g = src_argb4444[0] >> 4;
		uint8 r = src_argb4444[1] & 0x0f;
		uint8 a = src_argb4444[1] >> 4;
		dst_argb[0] = (b << 4) | b;
		dst_argb[1] = (g << 4) | g;
		dst_argb[2] = (r << 4) | r;
		dst_argb[3] = (a << 4) | a;
		dst_argb += 4;
		src_argb4444 += 2;
	}
}

// Cxbx color component conversion functions 
void X1R5G5B5ToARGBRow_C(const uint8* src_x1r5g5b5, uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_x1r5g5b5[0] & 0x1f;
		uint8 g = (src_x1r5g5b5[0] >> 5) | ((src_x1r5g5b5[1] & 0x03) << 3);
		uint8 r = (src_x1r5g5b5[1] & 0x7c) >> 2;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_x1r5g5b5 += 2;
	}
}

void A8R8G8B8ToARGBRow_C(const uint8* src_a8r8g8b8, uint8* dst_argb, int width) {
	memcpy(dst_argb, src_a8r8g8b8, width * sizeof(XTL::D3DCOLOR)); // Cxbx pass-through
}

void X8R8G8B8ToARGBRow_C(const uint8* src_x8r8g8b8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_x8r8g8b8[0];
		uint8 g = src_x8r8g8b8[1];
		uint8 r = src_x8r8g8b8[2];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = r;
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_x8r8g8b8 += 4;
	}
}

void ____R8B8ToARGBRow_C(const uint8* src_r8b8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_r8b8[0];
		uint8 r = src_r8b8[1];
		dst_argb[0] = b;
		dst_argb[1] = b;
		dst_argb[2] = r;
		dst_argb[3] = r;
		dst_argb += 4;
		src_r8b8 += 2;
	}
}

void ____G8B8ToARGBRow_C(const uint8* src_g8b8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_g8b8[0];
		uint8 g = src_g8b8[1];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = b;
		dst_argb[3] = g;
		dst_argb += 4;
		src_g8b8 += 2;
	}
}

void ______A8ToARGBRow_C(const uint8* src_a8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_a8[0];
		dst_argb[0] = 255u;
		dst_argb[1] = 255u;
		dst_argb[2] = 255u;
		dst_argb[3] = a;
		dst_argb += 4;
		src_a8 += 1;
	}
}

void __R6G5B5ToARGBRow_C(const uint8* src_r6g5b5, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_r6g5b5[0] & 0x1f;
		uint8 g = (src_r6g5b5[0] >> 5) | ((src_r6g5b5[1] & 0x03) << 3);
		uint8 r = src_r6g5b5[1] >> 2;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 2) | (r >> 4);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_r6g5b5 += 2;
	}
}

void R5G5B5A1ToARGBRow_C(const uint8* src_r5g5b5a1, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r5g5b5a1[0] & 1;
		uint8 b = (src_r5g5b5a1[0] & 0x3e) >> 1;
		uint8 g = (src_r5g5b5a1[0] >> 6) | ((src_r5g5b5a1[1] & 0x07) << 2);
		uint8 r = (src_r5g5b5a1[1] & 0xf8) >> 3;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = -a;
		dst_argb += 4;
		src_r5g5b5a1 += 2;
	}
}

void R4G4B4A4ToARGBRow_C(const uint8* src_r4g4b4a4, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r4g4b4a4[0] & 0x0f;
		uint8 b = src_r4g4b4a4[0] >> 4;
		uint8 g = src_r4g4b4a4[1] & 0x0f;
		uint8 r = src_r4g4b4a4[1] >> 4;
		dst_argb[0] = (b << 4) | b;
		dst_argb[1] = (g << 4) | g;
		dst_argb[2] = (r << 4) | r;
		dst_argb[3] = (a << 4) | a;
		dst_argb += 4;
		src_r4g4b4a4 += 2;
	}
}

void A8B8G8R8ToARGBRow_C(const uint8* src_a8b8g8r8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 r = src_a8b8g8r8[0];
		uint8 g = src_a8b8g8r8[1];
		uint8 b = src_a8b8g8r8[3];
		uint8 a = src_a8b8g8r8[4];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = r;
		dst_argb[3] = a;
		dst_argb += 4;
		src_a8b8g8r8 += 4;
	}
}

void B8G8R8A8ToARGBRow_C(const uint8* src_b8g8r8a8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_b8g8r8a8[0];
		uint8 r = src_b8g8r8a8[1];
		uint8 g = src_b8g8r8a8[3];
		uint8 b = src_b8g8r8a8[4];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = r;
		dst_argb[3] = a;
		dst_argb += 4;
		src_b8g8r8a8 += 4;
	}
}

void R8G8B8A8ToARGBRow_C(const uint8* src_r8g8b8a8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r8g8b8a8[0];
		uint8 b = src_r8g8b8a8[1];
		uint8 g = src_r8g8b8a8[3];
		uint8 r = src_r8g8b8a8[4];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = r;
		dst_argb[3] = a;
		dst_argb += 4;
		src_r8g8b8a8 += 4;
	}
}

void ______L8ToARGBRow_C(const uint8* src_l8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_l8[0];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_l8 += 1;
	}
}

void _____AL8ToARGBRow_C(const uint8* src_al8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_al8[0];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = l;
		dst_argb += 4;
		src_al8 += 1;
	}
}

void _____L16ToARGBRow_C(const uint8* src_l16, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_l16[0];
		uint8 g = src_l16[1];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = 255u;
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_l16 += 2;
	}
}

void ____A8L8ToARGBRow_C(const uint8* src_a8l8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_a8l8[0];
		uint8 a = src_a8l8[1];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = a;
		dst_argb += 4;
		src_a8l8 += 2;
	}
}

typedef struct TRGB32
{
	unsigned char B;
	unsigned char G;
	unsigned char R;
	unsigned char A;
} TRGB32;

// DXT1 info (MSDN Block Compression) : https://msdn.microsoft.com/en-us/library/bb694531.aspx
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb147243(v=vs.85).aspx
void ____DXT1ToARGBRow_C(const uint8* data, uint8* dst_argb, int width) {
	int dst_pitch = *(int*)dst_argb; // dirty hack to avoid another argument
	int x;
	for (x = 0; x < width; x+=4) {
		// Read two 16-bit pixels
		uint16 color_0 = data[0] | (data[1] << 8);
		uint16 color_1 = data[2] | (data[3] << 8);

		// Read 5+6+5 bit color channels
		uint8 b0 = color_0 & 0x1f;
		uint8 g0 = (color_0 >> 5) & 0x3f;
		uint8 r0 = color_0 >> 11;

		uint8 b1 = color_1 & 0x1f;
		uint8 g1 = (color_1 >> 5) & 0x3f;
		uint8 r1 = color_1 >> 11;

		// Build first half of RGB32 color map (converting 5+6+5 to 8+8+8):
		TRGB32 colormap[4];
		colormap[0].B = (b0 << 3) | (b0 >> 2);
		colormap[0].G = (g0 << 2) | (g0 >> 4);
		colormap[0].R = (r0 << 3) | (r0 >> 2);
		colormap[0].A = 255u;

		colormap[1].B = (b1 << 3) | (b1 >> 2);
		colormap[1].G = (g1 << 2) | (g1 >> 4);
		colormap[1].R = (r1 << 3) | (r1 >> 2);
		colormap[1].A = 255u;

		// Build second half of RGB32 color map :
		if (color_0 > color_1)
		{
			// Make up new color : 2/3 A + 1/3 B :
			colormap[2].B = (uint8)((2 * colormap[0].B + 1 * colormap[1].B + 2) / 3);
			colormap[2].G = (uint8)((2 * colormap[0].G + 1 * colormap[1].G + 2) / 3);
			colormap[2].R = (uint8)((2 * colormap[0].R + 1 * colormap[1].R + 2) / 3);
			colormap[2].A = 255u;
			// Make up new color : 1/3 A + 2/3 B :
			colormap[3].B = (uint8)((1 * colormap[0].B + 2 * colormap[1].B + 2) / 3);
			colormap[3].G = (uint8)((1 * colormap[0].G + 2 * colormap[1].G + 2) / 3);
			colormap[3].R = (uint8)((1 * colormap[0].R + 2 * colormap[1].R + 2) / 3);
			colormap[3].A = 255u;
		}
		else
		{
};

#else // !OLD_COLOR_CONVERSION

// Conversion functions copied from libyuv
// See https://chromium.googlesource.com/libyuv/libyuv/+/master/source/row_common.cc
void RGB565ToARGBRow_C(const uint8* src_rgb565, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_rgb565[0] & 0x1f;
		uint8 g = (src_rgb565[0] >> 5) | ((src_rgb565[1] & 0x07) << 3);
		uint8 r = src_rgb565[1] >> 3;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 2) | (g >> 4);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_rgb565 += 2;
	}
}
void ARGB1555ToARGBRow_C(const uint8* src_argb1555,
	uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_argb1555[0] & 0x1f;
		uint8 g = (src_argb1555[0] >> 5) | ((src_argb1555[1] & 0x03) << 3);
		uint8 r = (src_argb1555[1] & 0x7c) >> 2;
		uint8 a = src_argb1555[1] >> 7;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = -a;
		dst_argb += 4;
		src_argb1555 += 2;
	}
}
void ARGB4444ToARGBRow_C(const uint8* src_argb4444,
	uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_argb4444[0] & 0x0f;
		uint8 g = src_argb4444[0] >> 4;
		uint8 r = src_argb4444[1] & 0x0f;
		uint8 a = src_argb4444[1] >> 4;
		dst_argb[0] = (b << 4) | b;
		dst_argb[1] = (g << 4) | g;
		dst_argb[2] = (r << 4) | r;
		dst_argb[3] = (a << 4) | a;
		dst_argb += 4;
		src_argb4444 += 2;
	}
}

// Cxbx color component conversion functions 
void X1R5G5B5ToARGBRow_C(const uint8* src_x1r5g5b5, uint8* dst_argb,
	int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_x1r5g5b5[0] & 0x1f;
		uint8 g = (src_x1r5g5b5[0] >> 5) | ((src_x1r5g5b5[1] & 0x03) << 3);
		uint8 r = (src_x1r5g5b5[1] & 0x7c) >> 2;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_x1r5g5b5 += 2;
	}
}

void A8R8G8B8ToARGBRow_C(const uint8* src_a8r8g8b8, uint8* dst_argb, int width) {
	memcpy(dst_argb, src_a8r8g8b8, width * 4); // Cxbx pass-through
}

void X8R8G8B8ToARGBRow_C(const uint8* src_x8r8g8b8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_x8r8g8b8[0];
		uint8 g = src_x8r8g8b8[1];
		uint8 r = src_x8r8g8b8[2];
		dst_rgb[0] = b;
		dst_rgb[1] = g;
		dst_rgb[2] = r;
		dst_rgb[3] = 255u;
		dst_rgb += 4;
		src_x8r8g8b8 += 4;
	}
}

void ____R8B8ToARGBRow_C(const uint8* src_r8b8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_r8b8[0];
		uint8 r = src_r8b8[1];
		dst_rgb[0] = b;
		dst_rgb[1] = b;
		dst_rgb[2] = r;
		dst_rgb[3] = r;
		dst_rgb += 4;
		src_r8b8 += 2;
	}
}

void ____G8B8ToARGBRow_C(const uint8* src_g8b8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_g8b8[0];
		uint8 g = src_g8b8[1];
		dst_rgb[0] = b;
		dst_rgb[1] = g;
		dst_rgb[2] = b;
		dst_rgb[3] = g;
		dst_rgb += 4;
		src_g8b8 += 2;
	}
}

void ______A8ToARGBRow_C(const uint8* src_a8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_a8[0];
		dst_rgb[0] = 0u; // TODO : Use 255u ?
		dst_rgb[1] = 0u; // TODO : Use 255u ?
		dst_rgb[2] = 0u; // TODO : Use 255u ?
		dst_rgb[3] = a;
		dst_rgb += 4;
		src_a8 += 1;
	}
}

void __R6G5B5ToARGBRow_C(const uint8* src_r6g5b5, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_r6g5b5[0] & 0x1f;
		uint8 g = (src_r6g5b5[0] >> 5) | ((src_r6g5b5[1] & 0x03) << 3);
		uint8 r = src_r6g5b5[1] >> 2;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 2) | (r >> 4);
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_r6g5b5 += 2;
	}
}

void R5G5B5A1ToARGBRow_C(const uint8* src_r5g5b5a1, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r5g5b5a1[0] & 1;
		uint8 b = (src_r5g5b5a1[0] & 0x3e) >> 1;
		uint8 g = (src_r5g5b5a1[0] >> 6) | ((src_r5g5b5a1[1] & 0x07) << 2);
		uint8 r = (src_r5g5b5a1[1] & 0xf8) >> 3;
		dst_argb[0] = (b << 3) | (b >> 2);
		dst_argb[1] = (g << 3) | (g >> 2);
		dst_argb[2] = (r << 3) | (r >> 2);
		dst_argb[3] = -a;
		dst_argb += 4;
		src_r5g5b5a1 += 2;
	}
}

void R4G4B4A4ToARGBRow_C(const uint8* src_r4g4b4a4, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r4g4b4a4[0] & 0x0f;
		uint8 b = src_r4g4b4a4[0] >> 4;
		uint8 g = src_r4g4b4a4[1] & 0x0f;
		uint8 r = src_r4g4b4a4[1] >> 4;
		dst_argb[0] = (b << 4) | b;
		dst_argb[1] = (g << 4) | g;
		dst_argb[2] = (r << 4) | r;
		dst_argb[3] = (a << 4) | a;
		dst_argb += 4;
		src_r4g4b4a4 += 2;
	}
}

void A8B8G8R8ToARGBRow_C(const uint8* src_a8b8g8r8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 r = src_a8b8g8r8[0];
		uint8 g = src_a8b8g8r8[1];
		uint8 b = src_a8b8g8r8[3];
		uint8 a = src_a8b8g8r8[4];
		dst_rgb[0] = b;
		dst_rgb[1] = g;
		dst_rgb[2] = r;
		dst_rgb[3] = a;
		dst_rgb += 4;
		src_a8b8g8r8 += 4;
	}
}

void B8G8R8A8ToARGBRow_C(const uint8* src_b8g8r8a8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_b8g8r8a8[0];
		uint8 r = src_b8g8r8a8[1];
		uint8 g = src_b8g8r8a8[3];
		uint8 b = src_b8g8r8a8[4];
		dst_rgb[0] = b;
		dst_rgb[1] = g;
		dst_rgb[2] = r;
		dst_rgb[3] = a;
		dst_rgb += 4;
		src_b8g8r8a8 += 4;
	}
}

void R8G8B8A8ToARGBRow_C(const uint8* src_r8g8b8a8, uint8* dst_rgb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 a = src_r8g8b8a8[0];
		uint8 b = src_r8g8b8a8[1];
		uint8 g = src_r8g8b8a8[3];
		uint8 r = src_r8g8b8a8[4];
		dst_rgb[0] = b;
		dst_rgb[1] = g;
		dst_rgb[2] = r;
		dst_rgb[3] = a;
		dst_rgb += 4;
		src_r8g8b8a8 += 4;
	}
}

void ______L8ToARGBRow_C(const uint8* src_l8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_l8[0];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_l8 += 1;
	}
}

void _____AL8ToARGBRow_C(const uint8* src_al8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_al8[0];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = l;
		dst_argb += 4;
		src_al8 += 1;
	}
}

void _____L16ToARGBRow_C(const uint8* src_l16, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 b = src_l16[0];
		uint8 g = src_l16[1];
		dst_argb[0] = b;
		dst_argb[1] = g;
		dst_argb[2] = 255u;
		dst_argb[3] = 255u;
		dst_argb += 4;
		src_l16 += 2;
	}
}

void ____A8L8ToARGBRow_C(const uint8* src_a8l8, uint8* dst_argb, int width) {
	int x;
	for (x = 0; x < width; ++x) {
		uint8 l = src_a8l8[0];
		uint8 a = src_a8l8[1];
		dst_argb[0] = l;
		dst_argb[1] = l;
		dst_argb[2] = l;
		dst_argb[3] = a;
		dst_argb += 4;
		src_a8l8 += 2;
	}
}

static const XTL::FormatToARGBRow ComponentConverters[] = {
	nullptr, // NoCmpnts,
	ARGB1555ToARGBRow_C, // A1R5G5B5,
	X1R5G5B5ToARGBRow_C, // X1R5G5B5, // Test : Convert X into 255
	ARGB4444ToARGBRow_C, // A4R4G4B4,
	  RGB565ToARGBRow_C, // __R5G6B5, // NOTE : A=255
	A8R8G8B8ToARGBRow_C, // A8R8G8B8,
	X8R8G8B8ToARGBRow_C, // X8R8G8B8, // Test : Convert X into 255
	____R8B8ToARGBRow_C, // ____R8B8, // NOTE : A takes R, G takes B
	____G8B8ToARGBRow_C, // ____G8B8, // NOTE : A takes G, R takes B
	______A8ToARGBRow_C, // ______A8,
	__R6G5B5ToARGBRow_C, // __R6G5B5,
	R5G5B5A1ToARGBRow_C, // R5G5B5A1,
	R4G4B4A4ToARGBRow_C, // R4G4B4A4,
	A8B8G8R8ToARGBRow_C, // A8B8G8R8,
	B8G8R8A8ToARGBRow_C, // B8G8R8A8,
	R8G8B8A8ToARGBRow_C, // R8G8B8A8,
	______L8ToARGBRow_C, // ______L8, // NOTE : A=255, R=G=B= L
	_____AL8ToARGBRow_C, // _____AL8, // NOTE : A=R=G=B= L
	_____L16ToARGBRow_C, // _____L16, // NOTE : Actually G8B8, with A=R=255
	____A8L8ToARGBRow_C, // ____A8L8, // NOTE : R=G=B= L
};
#endif // !OLD_COLOR_CONVERSION

enum _FormatStorage {
	Undfnd = 0, // Undefined
	Linear,
	Swzzld, // Swizzled
	Cmprsd // Compressed
};

enum _FormatUsage {
	Texture = 0,
	RenderTarget,
	DepthBuffer
};

typedef struct _FormatInfo {
	uint8_t bits_per_pixel;
	_FormatStorage stored;
	_ComponentEncoding components;
	XTL::D3DFORMAT pc;
	_FormatUsage usage;
	char *warning;
} FormatInfo;

static const FormatInfo FormatInfos[] = {
	// Notes :
	// * This table should use a native format that most closely mirrors the Xbox format,
	// and (for now) MUST use a format that uses the same number of bits/bytes per pixel,
	// as otherwise locked buffers wouldn't be of the same size as on the Xbox, which
	// could lead to out-of-bounds access violations.
	// * Each format for which the host D3D8 doesn't have an identical native format,
	// and does specify color components (other than NoCmpnts), MUST specify this fact
	// in the warning field, so EmuXBFormatRequiresConversionToARGB can return a conversion
	// to ARGB is needed (which is implemented in EMUPATCH(D3DResource_Register).

	/* 0x00 X_D3DFMT_L8           */ {  8, Swzzld, ______L8, XTL::D3DFMT_L8        },
	/* 0x01 X_D3DFMT_AL8          */ {  8, Swzzld, _____AL8, XTL::D3DFMT_L8        , Texture, "X_D3DFMT_AL8 -> D3DFMT_L8" },
	/* 0x02 X_D3DFMT_A1R5G5B5     */ { 16, Swzzld, A1R5G5B5, XTL::D3DFMT_A1R5G5B5  },
	/* 0x03 X_D3DFMT_X1R5G5B5     */ { 16, Swzzld, X1R5G5B5, XTL::D3DFMT_X1R5G5B5  , RenderTarget },
	/* 0x04 X_D3DFMT_A4R4G4B4     */ { 16, Swzzld, A4R4G4B4, XTL::D3DFMT_A4R4G4B4  },
	/* 0x05 X_D3DFMT_R5G6B5       */ { 16, Swzzld, __R5G6B5, XTL::D3DFMT_R5G6B5    , RenderTarget },
	/* 0x06 X_D3DFMT_A8R8G8B8     */ { 32, Swzzld, A8R8G8B8, XTL::D3DFMT_A8R8G8B8  , RenderTarget },
	/* 0x07 X_D3DFMT_X8R8G8B8     */ { 32, Swzzld, X8R8G8B8, XTL::D3DFMT_X8R8G8B8  , RenderTarget }, // Alias : X_D3DFMT_X8L8V8U8 
	/* 0x08 undefined             */ {},
	/* 0x09 undefined             */ {},
	/* 0x0A undefined             */ {},
	/* 0x0B X_D3DFMT_P8           */ {  8, Swzzld, ______P8, XTL::D3DFMT_P8        }, // 8-bit palletized
	/* 0x0C X_D3DFMT_DXT1         */ {  4, Cmprsd, ____DXT1, XTL::D3DFMT_DXT1      }, // opaque/one-bit alpha // NOTE : DXT1 is half byte per pixel, so divide Size and Pitch calculations by two!
	/* 0x0D undefined             */ {},
	/* 0x0E X_D3DFMT_DXT3         */ {  8, Cmprsd, ____DXT3, XTL::D3DFMT_DXT3      }, // Alias : X_D3DFMT_DXT2 // linear alpha
	/* 0x0F X_D3DFMT_DXT5         */ {  8, Cmprsd, ____DXT5, XTL::D3DFMT_DXT5      }, // Alias : X_D3DFMT_DXT4 // interpolated alpha
	/* 0x10 X_D3DFMT_LIN_A1R5G5B5 */ { 16, Linear, A1R5G5B5, XTL::D3DFMT_A1R5G5B5  },
	/* 0x11 X_D3DFMT_LIN_R5G6B5   */ { 16, Linear, __R5G6B5, XTL::D3DFMT_R5G6B5    , RenderTarget },
	/* 0x12 X_D3DFMT_LIN_A8R8G8B8 */ { 32, Linear, A8R8G8B8, XTL::D3DFMT_A8R8G8B8  , RenderTarget },
	/* 0x13 X_D3DFMT_LIN_L8       */ {  8, Linear, ______L8, XTL::D3DFMT_L8        , RenderTarget },
	/* 0x14 undefined             */ {},
	/* 0x15 undefined             */ {},
	/* 0x16 X_D3DFMT_LIN_R8B8     */ { 16, Linear, ____R8B8, XTL::D3DFMT_R5G6B5    , Texture, "X_D3DFMT_LIN_R8B8 -> D3DFMT_R5G6B5" },
	/* 0x17 X_D3DFMT_LIN_G8B8     */ { 16, Linear, ____G8B8, XTL::D3DFMT_R5G6B5    , RenderTarget, "X_D3DFMT_LIN_G8B8 -> D3DFMT_R5G6B5" }, // Alias : X_D3DFMT_LIN_V8U8
	/* 0x18 undefined             */ {},
	/* 0x19 X_D3DFMT_A8           */ {  8, Swzzld, ______A8, XTL::D3DFMT_A8        },
	/* 0x1A X_D3DFMT_A8L8         */ { 16, Swzzld, ____A8L8, XTL::D3DFMT_A8L8      },
	/* 0x1B X_D3DFMT_LIN_AL8      */ {  8, Linear, _____AL8, XTL::D3DFMT_L8        , Texture, "X_D3DFMT_LIN_AL8 -> D3DFMT_L8" },
	/* 0x1C X_D3DFMT_LIN_X1R5G5B5 */ { 16, Linear, X1R5G5B5, XTL::D3DFMT_X1R5G5B5  , RenderTarget },
	/* 0x1D X_D3DFMT_LIN_A4R4G4B4 */ { 16, Linear, A4R4G4B4, XTL::D3DFMT_A4R4G4B4  },
	/* 0x1E X_D3DFMT_LIN_X8R8G8B8 */ { 32, Linear, X8R8G8B8, XTL::D3DFMT_X8R8G8B8  , RenderTarget }, // Alias : X_D3DFMT_LIN_X8L8V8U8
	/* 0x1F X_D3DFMT_LIN_A8       */ {  8, Linear, ______A8, XTL::D3DFMT_A8        },
	/* 0x20 X_D3DFMT_LIN_A8L8     */ { 16, Linear, ____A8L8, XTL::D3DFMT_A8L8      },
	/* 0x21 undefined             */ {},
	/* 0x22 undefined             */ {},
	/* 0x23 undefined             */ {},
	/* 0x24 X_D3DFMT_YUY2         */ { 16, Undfnd, NoCmpnts, XTL::D3DFMT_YUY2      },
	/* 0x25 X_D3DFMT_UYVY         */ { 16, Undfnd, NoCmpnts, XTL::D3DFMT_UYVY      },
	/* 0x26 undefined             */ {},
	/* 0x27 X_D3DFMT_L6V5U5       */ { 16, Swzzld, __R6G5B5, XTL::D3DFMT_L6V5U5    }, // Alias : X_D3DFMT_R6G5B5 // XQEMU NOTE : This might be signed
	/* 0x28 X_D3DFMT_V8U8         */ { 16, Swzzld, ____G8B8, XTL::D3DFMT_V8U8      }, // Alias : X_D3DFMT_G8B8 // XQEMU NOTE : This might be signed
	/* 0x29 X_D3DFMT_R8B8         */ { 16, Swzzld, ____R8B8, XTL::D3DFMT_R5G6B5    , Texture, "X_D3DFMT_R8B8 -> D3DFMT_R5G6B5" }, // XQEMU NOTE : This might be signed
	/* 0x2A X_D3DFMT_D24S8        */ { 32, Swzzld, NoCmpnts, XTL::D3DFMT_D24S8     , DepthBuffer },
	/* 0x2B X_D3DFMT_F24S8        */ { 32, Swzzld, NoCmpnts, XTL::D3DFMT_D24S8     , DepthBuffer, "X_D3DFMT_F24S8 -> D3DFMT_D24S8" }, // HACK : PC doesn't have D3DFMT_F24S8 (Float vs Int)
	/* 0x2C X_D3DFMT_D16          */ { 16, Swzzld, NoCmpnts, XTL::D3DFMT_D16       , DepthBuffer }, // Alias : X_D3DFMT_D16_LOCKABLE // TODO : D3DFMT_D16 is always lockable on Xbox; Should PC use D3DFMT_D16_LOCKABLE instead? Needs testcase.
	/* 0x2D X_D3DFMT_F16          */ { 16, Swzzld, NoCmpnts, XTL::D3DFMT_D16       , DepthBuffer, "X_D3DFMT_F16 -> D3DFMT_D16" }, // HACK : PC doesn't have D3DFMT_F16 (Float vs Int)
	/* 0x2E X_D3DFMT_LIN_D24S8    */ { 32, Linear, NoCmpnts, XTL::D3DFMT_D24S8     , DepthBuffer },
	/* 0x2F X_D3DFMT_LIN_F24S8    */ { 32, Linear, NoCmpnts, XTL::D3DFMT_D24S8     , DepthBuffer, "X_D3DFMT_LIN_F24S8 -> D3DFMT_D24S8" }, // HACK : PC doesn't have D3DFMT_F24S8 (Float vs Int)
	/* 0x30 X_D3DFMT_LIN_D16      */ { 16, Linear, NoCmpnts, XTL::D3DFMT_D16       , DepthBuffer }, // TODO : D3DFMT_D16 is always lockable on Xbox; Should PC use D3DFMT_D16_LOCKABLE instead? Needs testcase.
	/* 0x31 X_D3DFMT_LIN_F16      */ { 16, Linear, NoCmpnts, XTL::D3DFMT_D16       , DepthBuffer, "X_D3DFMT_LIN_F16 -> D3DFMT_D16" }, // HACK : PC doesn't have D3DFMT_F16 (Float vs Int)
	/* 0x32 X_D3DFMT_L16          */ { 16, Swzzld, _____L16, XTL::D3DFMT_A8L8      , Texture, "X_D3DFMT_L16 -> D3DFMT_A8L8" },
	/* 0x33 X_D3DFMT_V16U16       */ { 32, Swzzld, NoCmpnts, XTL::D3DFMT_V16U16    },
	/* 0x34 undefined             */ {},
	/* 0x35 X_D3DFMT_LIN_L16      */ { 16, Linear, _____L16, XTL::D3DFMT_A8L8      , Texture, "X_D3DFMT_LIN_L16 -> D3DFMT_A8L8" },
	/* 0x36 X_D3DFMT_LIN_V16U16   */ { 32, Linear, NoCmpnts, XTL::D3DFMT_V16U16    },
	/* 0x37 X_D3DFMT_LIN_L6V5U5   */ { 16, Linear, __R6G5B5, XTL::D3DFMT_L6V5U5    }, // Alias : X_D3DFMT_LIN_R6G5B5
	/* 0x38 X_D3DFMT_R5G5B5A1     */ { 16, Swzzld, R5G5B5A1, XTL::D3DFMT_A1R5G5B5  , Texture, "X_D3DFMT_R5G5B5A1 -> D3DFMT_A1R5G5B5" },
	/* 0x39 X_D3DFMT_R4G4B4A4     */ { 16, Swzzld, R4G4B4A4, XTL::D3DFMT_A4R4G4B4  , Texture, "X_D3DFMT_R4G4B4A4 -> D3DFMT_A4R4G4B4" },
	/* 0x3A X_D3DFMT_Q8W8V8U8     */ { 32, Swzzld, A8B8G8R8, XTL::D3DFMT_Q8W8V8U8  }, // Alias : X_D3DFMT_A8B8G8R8 // TODO : Needs testcase.
	/* 0x3B X_D3DFMT_B8G8R8A8     */ { 32, Swzzld, B8G8R8A8, XTL::D3DFMT_A8R8G8B8  , Texture, "X_D3DFMT_B8G8R8A8 -> D3DFMT_A8R8G8B8" },
	/* 0x3C X_D3DFMT_R8G8B8A8     */ { 32, Swzzld, R8G8B8A8, XTL::D3DFMT_A8R8G8B8  , Texture, "X_D3DFMT_R8G8B8A8 -> D3DFMT_A8R8G8B8" },
	/* 0x3D X_D3DFMT_LIN_R5G5B5A1 */ { 16, Linear, R5G5B5A1, XTL::D3DFMT_A1R5G5B5  , Texture, "X_D3DFMT_LIN_R5G5B5A1 -> D3DFMT_A1R5G5B5" },
	/* 0x3E X_D3DFMT_LIN_R4G4B4A4 */ { 16, Linear, R4G4B4A4, XTL::D3DFMT_A4R4G4B4  , Texture, "X_D3DFMT_LIN_R4G4B4A4 -> D3DFMT_A4R4G4B4" },
	/* 0x3F X_D3DFMT_LIN_A8B8G8R8 */ { 32, Linear, A8B8G8R8, XTL::D3DFMT_A8R8G8B8  , Texture, "X_D3DFMT_LIN_A8B8G8R8 -> D3DFMT_A8R8G8B8" },
	/* 0x40 X_D3DFMT_LIN_B8G8R8A8 */ { 32, Linear, B8G8R8A8, XTL::D3DFMT_A8R8G8B8  , Texture, "X_D3DFMT_LIN_B8G8R8A8 -> D3DFMT_A8R8G8B8" },
	/* 0x41 X_D3DFMT_LIN_R8G8B8A8 */ { 32, Linear, R8G8B8A8, XTL::D3DFMT_A8R8G8B8  , Texture, "X_D3DFMT_LIN_R8G8B8A8 -> D3DFMT_A8R8G8B8" },
#if 0
	/* 0x42 to 0x63 undefined     */ {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},
	/* 0x64 X_D3DFMT_VERTEXDATA   */ {  8, Linear, NoCmpnts, XTL::D3DFMT_VERTEXDATA },
	/* 0x65 X_D3DFMT_INDEX16      */ { 16, Linear, NoCmpnts, XTL::D3DFMT_INDEX16    }, // Dxbx addition : X_D3DFMT_INDEX16 is not an Xbox format, but used internally
#endif
};

const XTL::FormatToARGBRow XTL::EmuXBFormatComponentConverter(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		if (FormatInfos[Format].components != NoCmpnts)
			return ComponentConverters[FormatInfos[Format].components];

	return nullptr;
}

bool XTL::EmuXBFormatCanBeConvertedToARGB(X_D3DFORMAT Format)
{
	const FormatToARGBRow info = EmuXBFormatComponentConverter(Format);
	return (info != nullptr);
}

bool XTL::EmuXBFormatRequiresConversionToARGB(X_D3DFORMAT Format)
{
	// Conversion is required if there's ARGB conversion info present, and the format has a warning message
	if (EmuXBFormatCanBeConvertedToARGB(Format))
		if (FormatInfos[Format].warning != nullptr)
			return true;

	return false;
}

DWORD XTL::EmuXBFormatBitsPerPixel(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		if (FormatInfos[Format].bits_per_pixel > 0) // TODO : Remove this
			return FormatInfos[Format].bits_per_pixel;

	return 16; // TODO : 8
}

DWORD XTL::EmuXBFormatBytesPerPixel(X_D3DFORMAT Format)
{
	return ((EmuXBFormatBitsPerPixel(Format) + 4) / 8);
}

BOOL XTL::EmuXBFormatIsCompressed(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		return (FormatInfos[Format].stored == Cmprsd);

	return false;
}

BOOL XTL::EmuXBFormatIsLinear(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		return (FormatInfos[Format].stored == Linear);

	return (Format == X_D3DFMT_VERTEXDATA); // TODO : false;
}

BOOL XTL::EmuXBFormatIsSwizzled(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		return (FormatInfos[Format].stored == Swzzld);

	return false;
}

BOOL XTL::EmuXBFormatIsRenderTarget(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		return (FormatInfos[Format].usage == RenderTarget);

	return false;
}

BOOL XTL::EmuXBFormatIsDepthBuffer(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8)
		return (FormatInfos[Format].usage == DepthBuffer);

	return false;
}

namespace XTL
{

// lookup table for converting vertex count to primitive count
int EmuD3DVertexToPrimitive[X_D3DPT_POLYGON + 1][2] =
{
	{ 0, 0 }, // NULL
	{ 1, 0 }, // X_D3DPT_POINTLIST
	{ 2, 0 }, // X_D3DPT_LINELIST
	{ 1, 1 }, // X_D3DPT_LINELOOP
	{ 1, 1 }, // X_D3DPT_LINESTRIP
	{ 3, 0 }, // X_D3DPT_TRIANGLELIST
	{ 1, 2 }, // X_D3DPT_TRIANGLESTRIP
	{ 1, 2 }, // X_D3DPT_TRIANGLEFAN
	{ 4, 0 }, // X_D3DPT_QUADLIST
	{ 2, 2 }, // X_D3DPT_QUADSTRIP
	{ 1, 2 }, // X_D3DPT_POLYGON // Was { 1, 0 },
};

// Table of Xbox-to-PC and Value-to-String converters for all registered types :
const XBTypeInfo DxbxXBTypeInfo[] = {
	/*xt_Unknown=*/
    {"Unknown",                  &EmuXB2PC_Copy},

    /*xtBOOL=*/
	{"BOOL",                     &EmuXB2PC_Copy,                &BOOL2String},                  // Xbox = PC
	/*xtBYTE=*/
	{"BYTE",                     &EmuXB2PC_Copy},                                               // Xbox = PC
	/*xtD3DBLEND=*/
    {"D3DBLEND",                 &EmuXB2PC_D3DBLEND,            &X_D3DBLEND2String},
	/*xtD3DBLENDOP=*/
    {"D3DBLENDOP",               &EmuXB2PC_D3DBLENDOP,          &X_D3DBLENDOP2String},
	/*xtD3DCLEAR=*/
    {"D3DCLEAR",                 &EmuXB2PC_D3DCLEAR_FLAGS,      &X_D3DCLEAR2String},
	/*xtD3DCMPFUNC=*/
    {"D3DCMPFUNC",               &EmuXB2PC_D3DCMPFUNC,          &X_D3DCMPFUNC2String},
	/*xtD3DCOLOR=*/
    {"D3DCOLOR",                 &EmuXB2PC_Copy},                                               // Xbox = PC
	/*xtD3DCOLORWRITEENABLE=*/
    {"D3DCOLORWRITEENABLE",      &EmuXB2PC_D3DCOLORWRITEENABLE, &X_D3DCOLORWRITEENABLE2String},
	/*xtD3DCUBEMAP_FACES=*/
    {"D3DCUBEMAP_FACES",         &EmuXB2PC_Copy,                &X_D3DCUBEMAP_FACES2String},
	/*xtD3DCULL=*/
    {"D3DCULL",                  &EmuXB2PC_D3DCULL,             &X_D3DCULL2String},
	/*xtD3DDCC=*/
    {"D3DDCC",                   &EmuXB2PC_Copy,                &X_D3DDCC2String,               true},
    /*xtD3DFILLMODE=*/
	{"D3DFILLMODE",              &EmuXB2PC_D3DFILLMODE,         &X_D3DFILLMODE2String},
    /*xtD3DFOGMODE=*/
	{"D3DFOGMODE",               &EmuXB2PC_Copy,                &X_D3DFOGMODE2String},          // Xbox = PC
	/*xtD3DFORMAT=*/
    {"D3DFORMAT",                &EmuXB2PC_D3DFormat,           &X_D3DFORMAT2String},
	/*xtD3DFRONT=*/
    {"D3DFRONT",                 &EmuXB2PC_Copy,                &X_D3DFRONT2String,             true},
	/*xtD3DLOGICOP=*/
    {"D3DLOGICOP",               &EmuXB2PC_Copy,                &X_D3DLOGICOP2String,           true},
	/*xtD3DMCS=*/
    {"D3DMCS",                   &EmuXB2PC_Copy,                &X_D3DMCS2String},              // Xbox = PC
	/*xtD3DMULTISAMPLE_TYPE=*/
    {"D3DMULTISAMPLE_TYPE",      &EmuXB2PC_D3DMULTISAMPLE_TYPE, &X_D3DMULTISAMPLE_TYPE2String},
	/*xtD3DMULTISAMPLEMODE=*/
    {"D3DMULTISAMPLEMODE",       &EmuXB2PC_Copy,                &X_D3DMULTISAMPLEMODE2String,   true},
	/*xtD3DPRIMITIVETYPE=*/
    {"D3DPRIMITIVETYPE",         &EmuXB2PC_D3DPrimitiveType,    &X_D3DPRIMITIVETYPE2String},
	/*xtD3DRESOURCETYPE=*/
    {"D3DRESOURCETYPE",          &EmuXB2PC_Copy,                &X_D3DRESOURCETYPE2String},
	/*xtD3DSAMPLEALPHA=*/
    {"D3DSAMPLEALPHA",           &EmuXB2PC_Copy,                &X_D3DSAMPLEALPHA2String,       true},
	/*xtD3DSHADEMODE=*/
    {"D3DSHADEMODE",             &EmuXB2PC_D3DSHADEMODE,        &X_D3DSHADEMODE2String},
	/*xtD3DSTENCILOP=*/
    {"D3DSTENCILOP",             &EmuXB2PC_D3DSTENCILOP,        &X_D3DSTENCILOP2String},
	/*xtD3DSWATH=*/
    {"D3DSWATH",                 &EmuXB2PC_Copy,                &X_D3DSWATH2String,             true},
	/*xtD3DTA = */
	{"D3DTA",                    &EmuXB2PC_Copy,                &X_D3DTA2String},
	/*xtD3DTEXTUREADDRESS=*/ // Used for TextureStageState X_D3DTSS_ADDRESSU, X_D3DTSS_ADDRESSV and X_D3DTSS_ADDRESSW
    {"D3DTEXTUREADDRESS",        &EmuXB2PC_D3DTEXTUREADDRESS,   &X_D3DTEXTUREADDRESS2String},
	/*xtD3DTEXTUREFILTERTYPE=*/ // Used for TextureStageState X_D3DTSS_MAGFILTER, X_D3DTSS_MINFILTER and X_D3DTSS_MIPFILTER
    {"D3DTEXTUREFILTERTYPE",     &EmuXB2PC_D3DTEXTUREFILTERTYPE},
	/*xtD3DTEXTUREOP=*/ // Used for TextureStageState X_D3DTSS_COLOROP and X_D3DTSS_ALPHAOP
    {"D3DTEXTUREOP",             &EmuXB2PC_D3DTEXTUREOP,        &X_D3DTEXTUREOP2String},
	/*xtD3DTEXTURESTAGESTATETYPE = */
    {"D3DTEXTURESTAGESTATETYPE", &EmuXB2PC_D3DTSS,              &X_D3DTEXTURESTAGESTATETYPE2String},
	/*xtD3DTEXTURETRANSFORMFLAGS = */
	{"D3DTEXTURETRANSFORMFLAGS", &EmuXB2PC_Copy,                &X_D3DTEXTURETRANSFORMFLAGS2String},
	/*xtD3DTRANSFORMSTATETYPE = */
    {"D3DTRANSFORMSTATETYPE",    &EmuXB2PC_D3DTS,               &X_D3DTRANSFORMSTATETYPE2String},
	/*xtD3DTSS_TCI = */
	{"D3DTSS_TCI",               &EmuXB2PC_D3DTSS_TCI,          &X_D3DTEXTURECOORDINDEX2String },
	/*xtD3DVERTEXBLENDFLAGS = */
    {"D3DVERTEXBLENDFLAGS",      &EmuXB2PC_D3DVERTEXBLENDFLAGS, &X_D3DVERTEXBLENDFLAGS2String},
	/*xtD3DVSDE = */
    {"D3DVSDE",                  &EmuXB2PC_Copy,                &X_D3DVSDE2String},
	/*xtD3DWRAP = */
    {"D3DWRAP",                  &EmuXB2PC_D3DWRAP,             &X_D3DWRAP2String},
	/*xtDWORD = */
    {"DWORD",                    &EmuXB2PC_Copy},                                               // Xbox = PC
	/*xtFloat = */
    {"Float",                    &EmuXB2PC_Copy,                &DWFloat2String},               // Xbox = PC
	/*xtLONG = */
    {"LONG",                     &EmuXB2PC_Copy},                                               // Xbox = PC
};

// The below list of XDK-dependent RenderStates is entirely verified for 3911, 4627, 5558, 5788, 5849 and 5933
// XDK 4627 has a total of 143 render states,
// XDK 5558 and higher has 165 render states.
// TODO : Update this list with XDK versions in between (especially below 3911, 4361 and between 4627 and 5558)
const RenderStateInfo DxbxRenderStateInfo[] = {
	// String                                  Ord   Version Type                   Method              Native
	{ "X_D3DRS_PSALPHAINPUTS0"              /*=   0*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(0) },
	{ "X_D3DRS_PSALPHAINPUTS1"              /*=   1*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(1) },
	{ "X_D3DRS_PSALPHAINPUTS2"              /*=   2*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(2) },
	{ "X_D3DRS_PSALPHAINPUTS3"              /*=   3*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(3) },
	{ "X_D3DRS_PSALPHAINPUTS4"              /*=   4*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(4) },
	{ "X_D3DRS_PSALPHAINPUTS5"              /*=   5*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(5) },
	{ "X_D3DRS_PSALPHAINPUTS6"              /*=   6*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(6) },
	{ "X_D3DRS_PSALPHAINPUTS7"              /*=   7*/, 3424, xtDWORD,               NV2A_RC_IN_ALPHA(7) },
	{ "X_D3DRS_PSFINALCOMBINERINPUTSABCD"   /*=   8*/, 3424, xtDWORD,               NV2A_RC_FINAL0 },
	{ "X_D3DRS_PSFINALCOMBINERINPUTSEFG"    /*=   9*/, 3424, xtDWORD,               NV2A_RC_FINAL1 },
	{ "X_D3DRS_PSCONSTANT0_0"               /*=  10*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(0) },
	{ "X_D3DRS_PSCONSTANT0_1"               /*=  11*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(1) },
	{ "X_D3DRS_PSCONSTANT0_2"               /*=  12*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(2) },
	{ "X_D3DRS_PSCONSTANT0_3"               /*=  13*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(3) },
	{ "X_D3DRS_PSCONSTANT0_4"               /*=  14*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(4) },
	{ "X_D3DRS_PSCONSTANT0_5"               /*=  15*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(5) },
	{ "X_D3DRS_PSCONSTANT0_6"               /*=  16*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(6) },
	{ "X_D3DRS_PSCONSTANT0_7"               /*=  17*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(7) },
	{ "X_D3DRS_PSCONSTANT1_0"               /*=  18*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(0) },
	{ "X_D3DRS_PSCONSTANT1_1"               /*=  19*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(1) },
	{ "X_D3DRS_PSCONSTANT1_2"               /*=  20*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(2) },
	{ "X_D3DRS_PSCONSTANT1_3"               /*=  21*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(3) },
	{ "X_D3DRS_PSCONSTANT1_4"               /*=  22*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(4) },
	{ "X_D3DRS_PSCONSTANT1_5"               /*=  23*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(5) },
	{ "X_D3DRS_PSCONSTANT1_6"               /*=  24*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(6) },
	{ "X_D3DRS_PSCONSTANT1_7"               /*=  25*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR1(7) },
	{ "X_D3DRS_PSALPHAOUTPUTS0"             /*=  26*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(0) },
	{ "X_D3DRS_PSALPHAOUTPUTS1"             /*=  27*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(1) },
	{ "X_D3DRS_PSALPHAOUTPUTS2"             /*=  28*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(2) },
	{ "X_D3DRS_PSALPHAOUTPUTS3"             /*=  29*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(3) },
	{ "X_D3DRS_PSALPHAOUTPUTS4"             /*=  30*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(4) },
	{ "X_D3DRS_PSALPHAOUTPUTS5"             /*=  31*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(5) },
	{ "X_D3DRS_PSALPHAOUTPUTS6"             /*=  32*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(6) },
	{ "X_D3DRS_PSALPHAOUTPUTS7"             /*=  33*/, 3424, xtDWORD,               NV2A_RC_OUT_ALPHA(7) },
	{ "X_D3DRS_PSRGBINPUTS0"                /*=  34*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(0) },
	{ "X_D3DRS_PSRGBINPUTS1"                /*=  35*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(1) },
	{ "X_D3DRS_PSRGBINPUTS2"                /*=  36*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(2) },
	{ "X_D3DRS_PSRGBINPUTS3"                /*=  37*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(3) },
	{ "X_D3DRS_PSRGBINPUTS4"                /*=  38*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(4) },
	{ "X_D3DRS_PSRGBINPUTS5"                /*=  39*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(5) },
	{ "X_D3DRS_PSRGBINPUTS6"                /*=  40*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(6) },
	{ "X_D3DRS_PSRGBINPUTS7"                /*=  41*/, 3424, xtDWORD,               NV2A_RC_IN_RGB(7) },
	{ "X_D3DRS_PSCOMPAREMODE"               /*=  42*/, 3424, xtDWORD,               NV2A_TX_SHADER_CULL_MODE },
	{ "X_D3DRS_PSFINALCOMBINERCONSTANT0"    /*=  43*/, 3424, xtDWORD,               NV2A_RC_COLOR0 },
	{ "X_D3DRS_PSFINALCOMBINERCONSTANT1"    /*=  44*/, 3424, xtDWORD,               NV2A_RC_COLOR1 },
	{ "X_D3DRS_PSRGBOUTPUTS0"               /*=  45*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(0) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS1"               /*=  46*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(1) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS2"               /*=  47*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(2) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS3"               /*=  48*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(3) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS4"               /*=  49*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(4) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS5"               /*=  50*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(5) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS6"               /*=  51*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(6) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSRGBOUTPUTS7"               /*=  52*/, 3424, xtDWORD,               NV2A_RC_OUT_RGB(7) }, // TODO : Use xtD3DCOLOR ?
	{ "X_D3DRS_PSCOMBINERCOUNT"             /*=  53*/, 3424, xtDWORD,               NV2A_RC_ENABLE },
	{ "X_D3DRS_PS_RESERVED"                 /*=  54*/, 3424, xtDWORD,               NV2A_NOP }, // Dxbx note : This takes the slot of X_D3DPIXELSHADERDEF.PSTextureModes, set by D3DDevice_SetRenderState_LogicOp?
	{ "X_D3DRS_PSDOTMAPPING"                /*=  55*/, 3424, xtDWORD,               NV2A_TX_SHADER_DOTMAPPING },
	{ "X_D3DRS_PSINPUTTEXTURE"              /*=  56*/, 3424, xtDWORD,               NV2A_TX_SHADER_PREVIOUS },
	// End of "pixel-shader" render states, continuing with "simple" render states :
	{ "X_D3DRS_ZFUNC"                       /*=  57*/, 3424, xtD3DCMPFUNC,          NV2A_DEPTH_FUNC, D3DRS_ZFUNC },
	{ "X_D3DRS_ALPHAFUNC"                   /*=  58*/, 3424, xtD3DCMPFUNC,          NV2A_ALPHA_FUNC_FUNC, D3DRS_ALPHAFUNC },
	{ "X_D3DRS_ALPHABLENDENABLE"            /*=  59*/, 3424, xtBOOL,                NV2A_BLEND_FUNC_ENABLE, D3DRS_ALPHABLENDENABLE, "TRUE to enable alpha blending" },
	{ "X_D3DRS_ALPHATESTENABLE"             /*=  60*/, 3424, xtBOOL,                NV2A_ALPHA_FUNC_ENABLE, D3DRS_ALPHATESTENABLE, "TRUE to enable alpha tests" },
	{ "X_D3DRS_ALPHAREF"                    /*=  61*/, 3424, xtBYTE,                NV2A_ALPHA_FUNC_REF, D3DRS_ALPHAREF },
	{ "X_D3DRS_SRCBLEND"                    /*=  62*/, 3424, xtD3DBLEND,            NV2A_BLEND_FUNC_SRC, D3DRS_SRCBLEND },
	{ "X_D3DRS_DESTBLEND"                   /*=  63*/, 3424, xtD3DBLEND,            NV2A_BLEND_FUNC_DST, D3DRS_DESTBLEND },
	{ "X_D3DRS_ZWRITEENABLE"                /*=  64*/, 3424, xtBOOL,                NV2A_DEPTH_WRITE_ENABLE, D3DRS_ZWRITEENABLE, "TRUE to enable Z writes" },
	{ "X_D3DRS_DITHERENABLE"                /*=  65*/, 3424, xtBOOL,                NV2A_DITHER_ENABLE, D3DRS_DITHERENABLE, "TRUE to enable dithering" },
	{ "X_D3DRS_SHADEMODE"                   /*=  66*/, 3424, xtD3DSHADEMODE,        NV2A_SHADE_MODEL, D3DRS_SHADEMODE },
	{ "X_D3DRS_COLORWRITEENABLE"            /*=  67*/, 3424, xtD3DCOLORWRITEENABLE, NV2A_COLOR_MASK, D3DRS_COLORWRITEENABLE }, // *_ALPHA, etc. per-channel write enable
	{ "X_D3DRS_STENCILZFAIL"                /*=  68*/, 3424, xtD3DSTENCILOP,        NV2A_STENCIL_OP_ZFAIL, D3DRS_STENCILZFAIL, "Operation to do if stencil test passes and Z test fails" },
	{ "X_D3DRS_STENCILPASS"                 /*=  69*/, 3424, xtD3DSTENCILOP,        NV2A_STENCIL_OP_ZPASS, D3DRS_STENCILPASS, "Operation to do if both stencil and Z tests pass" },
	{ "X_D3DRS_STENCILFUNC"                 /*=  70*/, 3424, xtD3DCMPFUNC,          NV2A_STENCIL_FUNC_FUNC, D3DRS_STENCILFUNC },
	{ "X_D3DRS_STENCILREF"                  /*=  71*/, 3424, xtBYTE,                NV2A_STENCIL_FUNC_REF, D3DRS_STENCILREF, "BYTE reference value used in stencil test" },
	{ "X_D3DRS_STENCILMASK"                 /*=  72*/, 3424, xtBYTE,                NV2A_STENCIL_FUNC_MASK, D3DRS_STENCILMASK, "BYTE mask value used in stencil test" },
	{ "X_D3DRS_STENCILWRITEMASK"            /*=  73*/, 3424, xtBYTE,                NV2A_STENCIL_MASK, D3DRS_STENCILWRITEMASK, "BYTE write mask applied to values written to stencil buffer" },
	{ "X_D3DRS_BLENDOP"                     /*=  74*/, 3424, xtD3DBLENDOP,          NV2A_BLEND_EQUATION, D3DRS_BLENDOP },
#ifdef DXBX_USE_D3D9
	{ "X_D3DRS_BLENDCOLOR"                  /*=  75*/, 3424, xtD3DCOLOR,            NV2A_BLEND_COLOR, D3DRS_BLENDFACTOR, "D3DCOLOR for D3DBLEND_CONSTANTCOLOR" },
	// D3D9 D3DRS_BLENDFACTOR : D3DCOLOR used for a constant blend factor during alpha blending for devices that support D3DPBLENDCAPS_BLENDFACTOR
#else
	{ "X_D3DRS_BLENDCOLOR"                  /*=  75*/, 3424, xtD3DCOLOR,            NV2A_BLEND_COLOR, D3DRS_UNSUPPORTED, "D3DCOLOR for D3DBLEND_CONSTANTCOLOR" },
#endif
	{ "X_D3DRS_SWATHWIDTH"                  /*=  76*/, 3424, xtD3DSWATH,            NV2A_SWATH_WIDTH },
	{ "X_D3DRS_POLYGONOFFSETZSLOPESCALE"    /*=  77*/, 3424, xtFloat,               NV2A_POLYGON_OFFSET_FACTOR, D3DRS_UNSUPPORTED, "float Z factor for shadow maps" },
	{ "X_D3DRS_POLYGONOFFSETZOFFSET"        /*=  78*/, 3424, xtFloat,               NV2A_POLYGON_OFFSET_UNITS },
	{ "X_D3DRS_POINTOFFSETENABLE"           /*=  79*/, 3424, xtBOOL,                NV2A_POLYGON_OFFSET_POINT_ENABLE },
	{ "X_D3DRS_WIREFRAMEOFFSETENABLE"       /*=  80*/, 3424, xtBOOL,                NV2A_POLYGON_OFFSET_LINE_ENABLE },
	{ "X_D3DRS_SOLIDOFFSETENABLE"           /*=  81*/, 3424, xtBOOL,                NV2A_POLYGON_OFFSET_FILL_ENABLE },
	{ "X_D3DRS_DEPTHCLIPCONTROL"            /*=  82*/, 4432, xtD3DDCC,              NV2A_DEPTHCLIPCONTROL },
	{ "X_D3DRS_STIPPLEENABLE"               /*=  83*/, 4627, xtBOOL,                NV2A_POLYGON_STIPPLE_ENABLE },
	{ "X_D3DRS_SIMPLE_UNUSED8"              /*=  84*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED7"              /*=  85*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED6"              /*=  86*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED5"              /*=  87*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED4"              /*=  88*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED3"              /*=  89*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED2"              /*=  90*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_SIMPLE_UNUSED1"              /*=  91*/, 4627, xtDWORD,               0 },
	// End of "simple" render states, continuing with "deferred" render states :
	// Verified as XDK 3911 Deferred RenderStates (3424 yet to do)
	{ "X_D3DRS_FOGENABLE"                   /*=  92*/, 3424, xtBOOL,                NV2A_FOG_ENABLE, D3DRS_FOGENABLE }, // TRUE to enable fog blending
	{ "X_D3DRS_FOGTABLEMODE"                /*=  93*/, 3424, xtD3DFOGMODE,          NV2A_FOG_MODE, D3DRS_FOGTABLEMODE }, // D3DFOGMODE
	{ "X_D3DRS_FOGSTART"                    /*=  94*/, 3424, xtFloat,               NV2A_FOG_COORD_DIST, D3DRS_FOGSTART }, // float fog start (for both vertex and pixel fog)
	{ "X_D3DRS_FOGEND"                      /*=  95*/, 3424, xtFloat,               NV2A_FOG_MODE, D3DRS_FOGEND }, // float fog end
	{ "X_D3DRS_FOGDENSITY"                  /*=  96*/, 3424, xtFloat,               NV2A_FOG_EQUATION_CONSTANT, D3DRS_FOGDENSITY }, // float fog density // + NV2A_FOG_EQUATION_LINEAR + NV2A_FOG_EQUATION_QUADRATIC
	{ "X_D3DRS_RANGEFOGENABLE"              /*=  97*/, 3424, xtBOOL,                NV2A_FOG_COORD_DIST, D3DRS_RANGEFOGENABLE }, // TRUE to enable range-based fog
	{ "X_D3DRS_WRAP0"                       /*=  98*/, 3424, xtD3DWRAP,             NV2A_TX_WRAP(0), D3DRS_WRAP0 }, // D3DWRAP* flags (D3DWRAP_U, D3DWRAPCOORD_0, etc.) for 1st texture coord.
	{ "X_D3DRS_WRAP1"                       /*=  99*/, 3424, xtD3DWRAP,             NV2A_TX_WRAP(1), D3DRS_WRAP1 }, // D3DWRAP* flags (D3DWRAP_U, D3DWRAPCOORD_0, etc.) for 2nd texture coord.
	{ "X_D3DRS_WRAP2"                       /*= 100*/, 3424, xtD3DWRAP,             NV2A_TX_WRAP(2), D3DRS_WRAP2 }, // D3DWRAP* flags (D3DWRAP_U, D3DWRAPCOORD_0, etc.) for 3rd texture coord.
	{ "X_D3DRS_WRAP3"                       /*= 101*/, 3424, xtD3DWRAP,             NV2A_TX_WRAP(3), D3DRS_WRAP3 }, // D3DWRAP* flags (D3DWRAP_U, D3DWRAPCOORD_0, etc.) for 4th texture coord.
	{ "X_D3DRS_LIGHTING"                    /*= 102*/, 3424, xtBOOL,                NV2A_LIGHT_MODEL, D3DRS_LIGHTING }, // TRUE to enable lighting // TODO : Needs push-buffer data conversion
	{ "X_D3DRS_SPECULARENABLE"              /*= 103*/, 3424, xtBOOL,                NV2A_RC_FINAL0, D3DRS_SPECULARENABLE }, // TRUE to enable specular
	{ "X_D3DRS_LOCALVIEWER"                 /*= 104*/, 3424, xtBOOL,                0, D3DRS_LOCALVIEWER }, // TRUE to enable camera-relative specular highlights
	{ "X_D3DRS_COLORVERTEX"                 /*= 105*/, 3424, xtBOOL,                0, D3DRS_COLORVERTEX }, // TRUE to enable per-vertex color
	{ "X_D3DRS_BACKSPECULARMATERIALSOURCE"  /*= 106*/, 3424, xtD3DMCS,              0 }, // D3DMATERIALCOLORSOURCE (Xbox extension) nsp.
	{ "X_D3DRS_BACKDIFFUSEMATERIALSOURCE"   /*= 107*/, 3424, xtD3DMCS,              0 }, // D3DMATERIALCOLORSOURCE (Xbox extension) nsp.
	{ "X_D3DRS_BACKAMBIENTMATERIALSOURCE"   /*= 108*/, 3424, xtD3DMCS,              0 }, // D3DMATERIALCOLORSOURCE (Xbox extension) nsp.
	{ "X_D3DRS_BACKEMISSIVEMATERIALSOURCE"  /*= 109*/, 3424, xtD3DMCS,              0 }, // D3DMATERIALCOLORSOURCE (Xbox extension) nsp.
	{ "X_D3DRS_SPECULARMATERIALSOURCE"      /*= 110*/, 3424, xtD3DMCS,              NV2A_COLOR_MATERIAL, D3DRS_SPECULARMATERIALSOURCE }, // D3DMATERIALCOLORSOURCE
	{ "X_D3DRS_DIFFUSEMATERIALSOURCE"       /*= 111*/, 3424, xtD3DMCS,              0, D3DRS_DIFFUSEMATERIALSOURCE }, // D3DMATERIALCOLORSOURCE
	{ "X_D3DRS_AMBIENTMATERIALSOURCE"       /*= 112*/, 3424, xtD3DMCS,              0, D3DRS_AMBIENTMATERIALSOURCE }, // D3DMATERIALCOLORSOURCE
	{ "X_D3DRS_EMISSIVEMATERIALSOURCE"      /*= 113*/, 3424, xtD3DMCS,              0, D3DRS_EMISSIVEMATERIALSOURCE }, // D3DMATERIALCOLORSOURCE
	{ "X_D3DRS_BACKAMBIENT"                 /*= 114*/, 3424, xtD3DCOLOR,            NV2A_LIGHT_MODEL_BACK_SIDE_PRODUCT_AMBIENT_PLUS_EMISSION_R }, // D3DCOLOR (Xbox extension) // ..NV2A_MATERIAL_FACTOR_BACK_B nsp. Was NV2A_LIGHT_MODEL_BACK_AMBIENT_R
	{ "X_D3DRS_AMBIENT"                     /*= 115*/, 3424, xtD3DCOLOR,            NV2A_LIGHT_MODEL_FRONT_SIDE_PRODUCT_AMBIENT_PLUS_EMISSION_R, D3DRS_AMBIENT }, // D3DCOLOR // ..NV2A_LIGHT_MODEL_FRONT_AMBIENT_B + NV2A_MATERIAL_FACTOR_FRONT_R..NV2A_MATERIAL_FACTOR_FRONT_A  Was NV2A_LIGHT_MODEL_FRONT_AMBIENT_R
	{ "X_D3DRS_POINTSIZE"                   /*= 116*/, 3424, xtFloat,               NV2A_POINT_PARAMETER(0), D3DRS_POINTSIZE }, // float point size
	{ "X_D3DRS_POINTSIZE_MIN"               /*= 117*/, 3424, xtFloat,               0, D3DRS_POINTSIZE_MIN }, // float point size min threshold
	{ "X_D3DRS_POINTSPRITEENABLE"           /*= 118*/, 3424, xtBOOL,                NV2A_POINT_SMOOTH_ENABLE, D3DRS_POINTSPRITEENABLE }, // TRUE to enable point sprites
	{ "X_D3DRS_POINTSCALEENABLE"            /*= 119*/, 3424, xtBOOL,                NV2A_POINT_PARAMETERS_ENABLE, D3DRS_POINTSCALEENABLE }, // TRUE to enable point size scaling
	{ "X_D3DRS_POINTSCALE_A"                /*= 120*/, 3424, xtFloat,               0, D3DRS_POINTSCALE_A }, // float point attenuation A value
	{ "X_D3DRS_POINTSCALE_B"                /*= 121*/, 3424, xtFloat,               0, D3DRS_POINTSCALE_B }, // float point attenuation B value
	{ "X_D3DRS_POINTSCALE_C"                /*= 122*/, 3424, xtFloat,               0, D3DRS_POINTSCALE_C }, // float point attenuation C value
	{ "X_D3DRS_POINTSIZE_MAX"               /*= 123*/, 3424, xtFloat,               0, D3DRS_POINTSIZE_MAX }, // float point size max threshold
	{ "X_D3DRS_PATCHEDGESTYLE"              /*= 124*/, 3424, xtDWORD,               0, D3DRS_PATCHEDGESTYLE }, // D3DPATCHEDGESTYLE
	{ "X_D3DRS_PATCHSEGMENTS"               /*= 125*/, 3424, xtDWORD,               0, D3DRS_PATCHSEGMENTS }, // DWORD number of segments per edge when drawing patches
	// TODO -oDxbx : Is X_D3DRS_SWAPFILTER really a xtD3DMULTISAMPLE_TYPE?
	{ "X_D3DRS_SWAPFILTER"                  /*= 126*/, 4039, xtD3DMULTISAMPLE_TYPE, 0, D3DRS_UNSUPPORTED, "D3DTEXF_LINEAR etc. filter to use for Swap" }, // nsp.
	{ "X_D3DRS_PRESENTATIONINTERVAL"        /*= 127*/, 4627, xtDWORD,               0 }, // nsp. TODO : Use 4361?
	{ "X_D3DRS_DEFERRED_UNUSED8"            /*= 128*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED7"            /*= 129*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED6"            /*= 130*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED5"            /*= 131*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED4"            /*= 132*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED3"            /*= 133*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED2"            /*= 134*/, 4627, xtDWORD,               0 },
	{ "X_D3DRS_DEFERRED_UNUSED1"            /*= 135*/, 4627, xtDWORD,               0 },
	// End of "deferred" render states, continuing with "complex" render states :
	{ "X_D3DRS_PSTEXTUREMODES"              /*= 136*/, 3424, xtDWORD,               0 },
	{ "X_D3DRS_VERTEXBLEND"                 /*= 137*/, 3424, xtD3DVERTEXBLENDFLAGS, NV2A_SKIN_MODE, D3DRS_VERTEXBLEND },
	{ "X_D3DRS_FOGCOLOR"                    /*= 138*/, 3424, xtD3DCOLOR,            NV2A_FOG_COLOR, D3DRS_FOGCOLOR }, // SwapRgb
	{ "X_D3DRS_FILLMODE"                    /*= 139*/, 3424, xtD3DFILLMODE,         NV2A_POLYGON_MODE_FRONT, D3DRS_FILLMODE },
	{ "X_D3DRS_BACKFILLMODE"                /*= 140*/, 3424, xtD3DFILLMODE,         0 }, // nsp.
	{ "X_D3DRS_TWOSIDEDLIGHTING"            /*= 141*/, 3424, xtBOOL,                NV2A_POLYGON_MODE_BACK }, // nsp.
	{ "X_D3DRS_NORMALIZENORMALS"            /*= 142*/, 3424, xtBOOL,                NV2A_NORMALIZE_ENABLE, D3DRS_NORMALIZENORMALS },
	{ "X_D3DRS_ZENABLE"                     /*= 143*/, 3424, xtBOOL,                NV2A_DEPTH_TEST_ENABLE, D3DRS_ZENABLE }, // D3DZBUFFERTYPE?
	{ "X_D3DRS_STENCILENABLE"               /*= 144*/, 3424, xtBOOL,                NV2A_STENCIL_ENABLE, D3DRS_STENCILENABLE },
	{ "X_D3DRS_STENCILFAIL"                 /*= 145*/, 3424, xtD3DSTENCILOP,        NV2A_STENCIL_OP_FAIL, D3DRS_STENCILFAIL },
	{ "X_D3DRS_FRONTFACE"                   /*= 146*/, 3424, xtD3DFRONT,            NV2A_FRONT_FACE }, // nsp.
	{ "X_D3DRS_CULLMODE"                    /*= 147*/, 3424, xtD3DCULL,             NV2A_CULL_FACE, D3DRS_CULLMODE },
	{ "X_D3DRS_TEXTUREFACTOR"               /*= 148*/, 3424, xtD3DCOLOR,            NV2A_RC_CONSTANT_COLOR0(0), D3DRS_TEXTUREFACTOR },
#ifdef DXBX_USE_D3D9
	{ "X_D3DRS_ZBIAS"                       /*= 149*/, 3424, xtLONG,                0, D3DRS_DEPTHBIAS },
#else
	{ "X_D3DRS_ZBIAS"                       /*= 149*/, 3424, xtLONG,                0, D3DRS_ZBIAS },
#endif
	{ "X_D3DRS_LOGICOP"                     /*= 150*/, 3424, xtD3DLOGICOP,          NV2A_COLOR_LOGIC_OP_OP }, // nsp.
#ifdef DXBX_USE_D3D9
	{ "X_D3DRS_EDGEANTIALIAS"               /*= 151*/, 3424, xtBOOL,                NV2A_LINE_SMOOTH_ENABLE, D3DRS_ANTIALIASEDLINEENABLE }, // Dxbx note : No Xbox ext. (according to Direct3D8) !
#else
	{ "X_D3DRS_EDGEANTIALIAS"               /*= 151*/, 3424, xtBOOL,                NV2A_LINE_SMOOTH_ENABLE, D3DRS_EDGEANTIALIAS }, // Dxbx note : No Xbox ext. (according to Direct3D8) !
#endif
	{ "X_D3DRS_MULTISAMPLEANTIALIAS"        /*= 152*/, 3424, xtBOOL,                NV2A_MULTISAMPLE_CONTROL, D3DRS_MULTISAMPLEANTIALIAS },
	{ "X_D3DRS_MULTISAMPLEMASK"             /*= 153*/, 3424, xtDWORD,               NV2A_MULTISAMPLE_CONTROL, D3DRS_MULTISAMPLEMASK },
// For D3DRS_MULTISAMPLETYPE, see DxbxRenderStateInfo_D3DRS_MULTISAMPLETYPE_below_4039
//  { "X_D3DRS_MULTISAMPLETYPE"             /*= 154*/, 3424, xtD3DMULTISAMPLE_TYPE, 0 }, // [-4039> \_ aliasses  D3DMULTISAMPLE_TYPE - 
	{ "X_D3DRS_MULTISAMPLEMODE"             /*= 154*/, 4039, xtD3DMULTISAMPLEMODE,  0 }, // [4039+] /            D3DMULTISAMPLEMODE for the backbuffer
	{ "X_D3DRS_MULTISAMPLERENDERTARGETMODE" /*= 155*/, 4039, xtD3DMULTISAMPLEMODE,  NV2A_RT_FORMAT },
	{ "X_D3DRS_SHADOWFUNC"                  /*= 156*/, 3424, xtD3DCMPFUNC,          NV2A_TX_RCOMP },
	{ "X_D3DRS_LINEWIDTH"                   /*= 157*/, 3424, xtFloat,               NV2A_LINE_WIDTH },
	{ "X_D3DRS_SAMPLEALPHA"                 /*= 158*/, 4627, xtD3DSAMPLEALPHA,      0 }, // TODO : Not in 4531, but earlier then 4627?
	{ "X_D3DRS_DXT1NOISEENABLE"             /*= 159*/, 3424, xtBOOL,                NV2A_CLEAR_DEPTH_VALUE },
	{ "X_D3DRS_YUVENABLE"                   /*= 160*/, 3911, xtBOOL,                NV2A_CONTROL0 },
	{ "X_D3DRS_OCCLUSIONCULLENABLE"         /*= 161*/, 3911, xtBOOL,                NV2A_OCCLUDE_ZSTENCIL_EN },
	{ "X_D3DRS_STENCILCULLENABLE"           /*= 162*/, 3911, xtBOOL,                NV2A_OCCLUDE_ZSTENCIL_EN },
	{ "X_D3DRS_ROPZCMPALWAYSREAD"           /*= 163*/, 3911, xtBOOL,                0 },
	{ "X_D3DRS_ROPZREAD"                    /*= 164*/, 3911, xtBOOL,                0 },
	{ "X_D3DRS_DONOTCULLUNCOMPRESSED"       /*= 165*/, 3911, xtBOOL,                0 }
};

const RenderStateInfo DxbxRenderStateInfo_D3DRS_MULTISAMPLETYPE_below_4039 =
	{ "X_D3DRS_MULTISAMPLETYPE"             /*= 154*/, 3424, xtD3DMULTISAMPLE_TYPE, 0 }; // [-3911] \_ aliasses  D3DMULTISAMPLE_TYPE

const RenderStateInfo &GetDxbxRenderStateInfo(int State)
{
	if (State == X_D3DRS_MULTISAMPLETYPE)
		if (g_BuildVersion < 4039)
			return DxbxRenderStateInfo_D3DRS_MULTISAMPLETYPE_below_4039;

	return DxbxRenderStateInfo[State];
}

/*Direct3D8 states unused :
	D3DRS_LINEPATTERN
	D3DRS_LASTPIXEL
	D3DRS_ZVISIBLE
	D3DRS_WRAP4
	D3DRS_WRAP5
	D3DRS_WRAP6
	D3DRS_WRAP7
	D3DRS_CLIPPING
	D3DRS_FOGVERTEXMODE
	D3DRS_CLIPPLANEENABLE
	D3DRS_SOFTWAREVERTEXPROCESSING
	D3DRS_DEBUGMONITORTOKEN
	D3DRS_INDEXEDVERTEXBLENDENABLE
	D3DRS_TWEENFACTOR
	D3DRS_POSITIONORDER
	D3DRS_NORMALORDER

Direct3D9 states unused :
	D3DRS_SCISSORTESTENABLE = 174,
	D3DRS_SLOPESCALEDEPTHBIAS = 175,
	D3DRS_ANTIALIASEDLINEENABLE = 176,
	D3DRS_MINTESSELLATIONLEVEL = 178,
	D3DRS_MAXTESSELLATIONLEVEL = 179,
	D3DRS_ADAPTIVETESS_X = 180,
	D3DRS_ADAPTIVETESS_Y = 181,
	D3DRS_ADAPTIVETESS_Z = 182,
	D3DRS_ADAPTIVETESS_W = 183,
	D3DRS_ENABLEADAPTIVETESSELLATION = 184,
	D3DRS_TWOSIDEDSTENCILMODE = 185,   // BOOL enable/disable 2 sided stenciling
	D3DRS_CCW_STENCILFAIL = 186,   // D3DSTENCILOP to do if ccw stencil test fails
	D3DRS_CCW_STENCILZFAIL = 187,   // D3DSTENCILOP to do if ccw stencil test passes and Z test fails
	D3DRS_CCW_STENCILPASS = 188,   // D3DSTENCILOP to do if both ccw stencil and Z tests pass
	D3DRS_CCW_STENCILFUNC = 189,   // D3DCMPFUNC fn.  ccw Stencil Test passes if ((ref & mask) stencilfn (stencil & mask)) is true
	D3DRS_COLORWRITEENABLE1 = 190,   // Additional ColorWriteEnables for the devices that support D3DPMISCCAPS_INDEPENDENTWRITEMASKS
	D3DRS_COLORWRITEENABLE2 = 191,   // Additional ColorWriteEnables for the devices that support D3DPMISCCAPS_INDEPENDENTWRITEMASKS
	D3DRS_COLORWRITEENABLE3 = 192,   // Additional ColorWriteEnables for the devices that support D3DPMISCCAPS_INDEPENDENTWRITEMASKS
	D3DRS_SRGBWRITEENABLE = 194,   // Enable rendertarget writes to be DE-linearized to SRGB (for formats that expose D3DUSAGE_QUERY_SRGBWRITE)
	D3DRS_DEPTHBIAS = 195,
	D3DRS_WRAP8 = 198,   // Additional wrap states for vs_3_0+ attributes with D3DDECLUSAGE_TEXCOORD
	D3DRS_WRAP9 = 199,
	D3DRS_WRAP10 = 200,
	D3DRS_WRAP11 = 201,
	D3DRS_WRAP12 = 202,
	D3DRS_WRAP13 = 203,
	D3DRS_WRAP14 = 204,
	D3DRS_WRAP15 = 205,
	D3DRS_SEPARATEALPHABLENDENABLE = 206,  // TRUE to enable a separate blending function for the alpha channel
	D3DRS_SRCBLENDALPHA = 207,  // SRC blend factor for the alpha channel when D3DRS_SEPARATEDESTALPHAENABLE is TRUE
	D3DRS_DESTBLENDALPHA = 208,  // DST blend factor for the alpha channel when D3DRS_SEPARATEDESTALPHAENABLE is TRUE
	D3DRS_BLENDOPALPHA = 209   // Blending operation for the alpha channel when D3DRS_SEPARATEDESTALPHAENABLE is TRUE
*/

// TODO : Declare and implement these :
#define xtD3DTEXTURECOLORKEYOP xt_Unknown
#define xtD3DTSIGN xt_Unknown
#define xtD3DTEXTUREALPHAKILL xt_Unknown

const TextureStageStateInfo DxbxTextureStageStateInfo[] = {
//:array[X_D3DTSS_FIRST..X_D3DTSS_UNSUPPORTED] of TextureStageStateInfo = (
    //  String                         Ord   Type                            Native (defaults to D3DSAMP_UNSUPPORTED)
    {"X_D3DTSS_ADDRESSU"              /*= 0*/, xtD3DTEXTUREADDRESS,          D3DSAMP_ADDRESSU},
    {"X_D3DTSS_ADDRESSV"              /*= 1*/, xtD3DTEXTUREADDRESS,          D3DSAMP_ADDRESSV},
    {"X_D3DTSS_ADDRESSW"              /*= 2*/, xtD3DTEXTUREADDRESS,          D3DSAMP_ADDRESSW},
    {"X_D3DTSS_MAGFILTER"             /*= 3*/, xtD3DTEXTUREFILTERTYPE,       D3DSAMP_MAGFILTER},
    {"X_D3DTSS_MINFILTER"             /*= 4*/, xtD3DTEXTUREFILTERTYPE,       D3DSAMP_MINFILTER},
    {"X_D3DTSS_MIPFILTER"             /*= 5*/, xtD3DTEXTUREFILTERTYPE,       D3DSAMP_MIPFILTER},
    {"X_D3DTSS_MIPMAPLODBIAS"         /*= 6*/, xtFloat,                      D3DSAMP_MIPMAPLODBIAS},
    {"X_D3DTSS_MAXMIPLEVEL"           /*= 7*/, xtDWORD,                      D3DSAMP_MAXMIPLEVEL},
    {"X_D3DTSS_MAXANISOTROPY"         /*= 8*/, xtDWORD,                      D3DSAMP_MAXANISOTROPY},
    {"X_D3DTSS_COLORKEYOP"            /*= 9*/, xtD3DTEXTURECOLORKEYOP,       },
    {"X_D3DTSS_COLORSIGN"             /*=10*/, xtD3DTSIGN,                   },
    {"X_D3DTSS_ALPHAKILL"             /*=11*/, xtD3DTEXTUREALPHAKILL,        },
    {"X_D3DTSS_COLOROP"               /*=12*/, xtD3DTEXTUREOP,               (D3DSAMPLERSTATETYPE)D3DTSS_COLOROP},
    {"X_D3DTSS_COLORARG0"             /*=13*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_COLORARG0},
    {"X_D3DTSS_COLORARG1"             /*=14*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_COLORARG1},
    {"X_D3DTSS_COLORARG2"             /*=15*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_COLORARG2},
    {"X_D3DTSS_ALPHAOP"               /*=16*/, xtD3DTEXTUREOP,               (D3DSAMPLERSTATETYPE)D3DTSS_ALPHAOP},
    {"X_D3DTSS_ALPHAARG0"             /*=17*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_ALPHAARG0},
    {"X_D3DTSS_ALPHAARG1"             /*=18*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_ALPHAARG1},
    {"X_D3DTSS_ALPHAARG2"             /*=19*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_ALPHAARG2},
    {"X_D3DTSS_RESULTARG"             /*=20*/, xtD3DTA,                      (D3DSAMPLERSTATETYPE)D3DTSS_RESULTARG},
    {"X_D3DTSS_TEXTURETRANSFORMFLAGS" /*=21*/, xtD3DTEXTURETRANSFORMFLAGS,   (D3DSAMPLERSTATETYPE)D3DTSS_TEXTURETRANSFORMFLAGS},
    // End of "deferred" texture states, continuing with the rest :
    {"X_D3DTSS_BUMPENVMAT00"          /*=22*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVMAT00},
    {"X_D3DTSS_BUMPENVMAT01"          /*=23*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVMAT01},
    {"X_D3DTSS_BUMPENVMAT11"          /*=24*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVMAT11},
    {"X_D3DTSS_BUMPENVMAT10"          /*=25*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVMAT10},
    {"X_D3DTSS_BUMPENVLSCALE"         /*=26*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVLSCALE},
    {"X_D3DTSS_BUMPENVLOFFSET"        /*=27*/, xtFloat,                      (D3DSAMPLERSTATETYPE)D3DTSS_BUMPENVLOFFSET},
    {"X_D3DTSS_TEXCOORDINDEX"         /*=28*/, xtD3DTSS_TCI,                 (D3DSAMPLERSTATETYPE)D3DTSS_TEXCOORDINDEX},
    {"X_D3DTSS_BORDERCOLOR"           /*=29*/, xtD3DCOLOR,                   D3DSAMP_BORDERCOLOR},
    {"X_D3DTSS_COLORKEYCOLOR"         /*=30*/, xtD3DCOLOR,                   },
	{"unsupported"                    /*=31*/, xtDWORD,                      },
};

std::string BOOL2String(DWORD Value)
{
	if (Value > 0)
		return "true";
	
	return "false";
}

std::string DxbxXBDefaultToString(DWORD Value)
{
	return std::to_string(Value);
}

#define ForwardTYPE2PCHARToString(Type) \
std::string Type##2String(DWORD Value) { \
	std::string Result = TYPE2PCHAR(Type)((Type)Value); \
	return Result; }

ForwardTYPE2PCHARToString(X_D3DCULL)
ForwardTYPE2PCHARToString(X_D3DFORMAT)
ForwardTYPE2PCHARToString(X_D3DPRIMITIVETYPE)
ForwardTYPE2PCHARToString(X_D3DRESOURCETYPE)

/* TODO : Back-port these
X_D3DBLEND2String
X_D3DBLENDOP2String
X_D3DCLEAR2String
X_D3DCMPFUNC2String
X_D3DCOLORWRITEENABLE2String
X_D3DCUBEMAP_FACES2String
//X_D3DCULL2String
X_D3DDCC2String
X_D3DFILLMODE2String
X_D3DFOGMODE2String
//X_D3DFORMAT2String
X_D3DFRONT2String
X_D3DLOGICOP2String
X_D3DMCS2String
X_D3DMULTISAMPLE_TYPE2String
X_D3DMULTISAMPLEMODE2String
//X_D3DPRIMITIVETYPE2String
//X_D3DRESOURCETYPE2String
X_D3DSAMPLEALPHA2String
X_D3DSHADEMODE2String
X_D3DSTENCILOP2String
X_D3DSWATH2String
X_D3DTEXTUREADDRESS2String
X_D3DTEXTUREOP2String
X_D3DTEXTURESTAGESTATETYPE2String
X_D3DTRANSFORMSTATETYPE2String
X_D3DVERTEXBLENDFLAGS2String
X_D3DVSDE2String
X_D3DWRAP2String
*/

std::string DWFloat2String(DWORD Value)
{
	return std::to_string(*((FLOAT*)&Value)); // TODO : Speed this up by avoiding Single>Extended cast & generic render code.
}

DWORD EmuXB2PC_Copy(DWORD Value)
{
	return Value;
}

// convert from xbox to pc color formats
D3DFORMAT EmuXB2PC_D3DFormat(X_D3DFORMAT Format)
{
	if (Format <= X_D3DFMT_LIN_R8G8B8A8 && Format != -1 /*X_D3DFMT_UNKNOWN*/) // The last bit prevents crashing (Metal Slug 3)
	{
		const FormatInfo *info = &FormatInfos[Format];
		if (info->warning != nullptr)
			EmuWarning(info->warning);

		return info->pc;
	}

	switch (Format) {
	case X_D3DFMT_VERTEXDATA:
		return D3DFMT_VERTEXDATA;
	case ((X_D3DFORMAT)0xffffffff):
		return D3DFMT_UNKNOWN; // TODO -oCXBX: Not sure if this counts as swizzled or not...
	default:
		CxbxKrnlCleanup("Unknown X_D3DFORMAT (0x%.08X)", (DWORD)Format);
		return D3DFMT_UNKNOWN; // Never reached
	}
}

// convert from pc to xbox color formats
X_D3DFORMAT EmuPC2XB_D3DFormat(D3DFORMAT Format)
{
	X_D3DFORMAT result;
    switch(Format)
    {
	case D3DFMT_YUY2:
		result = X_D3DFMT_YUY2;
		break;
	case D3DFMT_UYVY:
		result = X_D3DFMT_UYVY;
		break;
	case D3DFMT_R5G6B5:
		result = X_D3DFMT_LIN_R5G6B5;
		break; // Linear
			   //      Result := X_D3DFMT_R5G6B5; // Swizzled

	case D3DFMT_D24S8:
		result = X_D3DFMT_D24S8;
		break; // Swizzled

	case D3DFMT_DXT5:
		result = X_D3DFMT_DXT5;
		break; // Compressed

	case D3DFMT_DXT4:
		result = X_D3DFMT_DXT4; // Same as X_D3DFMT_DXT5
		break; // Compressed

	case D3DFMT_DXT3:
		result = X_D3DFMT_DXT3;
		break; // Compressed

	case D3DFMT_DXT2:
		result = X_D3DFMT_DXT2; // Same as X_D3DFMT_DXT3
		break; // Compressed

	case D3DFMT_DXT1:
		result = X_D3DFMT_DXT1;
		break; // Compressed

	case D3DFMT_A1R5G5B5:
		result = X_D3DFMT_LIN_A1R5G5B5;
		break; // Linear

	case D3DFMT_X8R8G8B8:
		result = X_D3DFMT_LIN_X8R8G8B8;
		break; // Linear
			   //      Result := X_D3DFMT_X8R8G8B8; // Swizzled

	case D3DFMT_A8R8G8B8:
		//      Result := X_D3DFMT_LIN_A8R8G8B8; // Linear
		result = X_D3DFMT_A8R8G8B8;
		break;
	case D3DFMT_A4R4G4B4:
		result = X_D3DFMT_LIN_A4R4G4B4;
		break; // Linear
			   //      Result := X_D3DFMT_A4R4G4B4; // Swizzled
	case D3DFMT_X1R5G5B5:	// Linear
		result = X_D3DFMT_LIN_X1R5G5B5;
		break;
	case D3DFMT_A8:
		result = X_D3DFMT_A8;
		break;
	case D3DFMT_L8:
		result = X_D3DFMT_LIN_L8;
		break; // Linear
			   //        Result := X_D3DFMT_L8; // Swizzled

	case D3DFMT_D16: case D3DFMT_D16_LOCKABLE:
		result = X_D3DFMT_D16_LOCKABLE;
		break; // Swizzled

	case D3DFMT_UNKNOWN:
		result = ((X_D3DFORMAT)0xffffffff);
		break;

		// Dxbx additions :

	case D3DFMT_L6V5U5:
		result = X_D3DFMT_L6V5U5;
		break; // Swizzled

	case D3DFMT_V8U8:
		result = X_D3DFMT_V8U8;
		break; // Swizzled

	case D3DFMT_V16U16:
		result = X_D3DFMT_V16U16;
		break; // Swizzled

	case D3DFMT_VERTEXDATA:
		result = X_D3DFMT_VERTEXDATA;
		break;

	default:
		CxbxKrnlCleanup("Unknown D3DFORMAT (0x%.08X)", (DWORD)Format);
		// Never reached
    }

    return result;
}

// convert from xbox to pc d3d lock flags
DWORD EmuXB2PC_D3DLock(DWORD Flags)
{
    DWORD NewFlags = 0;

    // Need to convert the flags, TODO: fix the xbox extensions
//    if(Flags & X_D3DLOCK_NOFLUSH)
//        NewFlags ^= 0;

	if(Flags & X_D3DLOCK_NOOVERWRITE)
        NewFlags |= D3DLOCK_NOOVERWRITE;

//	if(Flags & X_D3DLOCK_TILED)
//        NewFlags ^= 0;

	if(Flags & X_D3DLOCK_READONLY)
        NewFlags |= D3DLOCK_READONLY;

    return NewFlags;
}

// convert from xbox to pc multisample formats
D3DMULTISAMPLE_TYPE EmuXB2PC_D3DMULTISAMPLE_TYPE(X_D3DMULTISAMPLE_TYPE Value)
{
	switch ((DWORD)Value & 0xFFFF) {
	case X_D3DMULTISAMPLE_NONE:
		return D3DMULTISAMPLE_NONE;
	case X_D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_LINEAR: 
		return D3DMULTISAMPLE_2_SAMPLES;
	case X_D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_QUINCUNX:
		return D3DMULTISAMPLE_2_SAMPLES;
	case X_D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_HORIZONTAL_LINEAR:
		return D3DMULTISAMPLE_2_SAMPLES;
	case X_D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_VERTICAL_LINEAR:
		return D3DMULTISAMPLE_2_SAMPLES;
	case X_D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_LINEAR: 
		return D3DMULTISAMPLE_4_SAMPLES;
	case X_D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_GAUSSIAN:
		return D3DMULTISAMPLE_4_SAMPLES;
	case X_D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_LINEAR:
		return D3DMULTISAMPLE_4_SAMPLES;
	case X_D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN:
		return D3DMULTISAMPLE_4_SAMPLES;
	case X_D3DMULTISAMPLE_9_SAMPLES_MULTISAMPLE_GAUSSIAN: 
		return D3DMULTISAMPLE_9_SAMPLES;
	case X_D3DMULTISAMPLE_9_SAMPLES_SUPERSAMPLE_GAUSSIAN:
		return D3DMULTISAMPLE_9_SAMPLES;
	default:
		EmuWarning("Unknown X_D3DMULTISAMPLE_TYPE (0x%.08X). Using D3DMULTISAMPLE_NONE approximation.", (DWORD)Value);
		return D3DMULTISAMPLE_NONE;
	}
}

// convert from xbox to pc texture transform state types
D3DTRANSFORMSTATETYPE EmuXB2PC_D3DTS(X_D3DTRANSFORMSTATETYPE Value)
{
	switch (Value) {
	case X_D3DTS_VIEW:
		return D3DTS_VIEW;
	case X_D3DTS_PROJECTION:
		return D3DTS_PROJECTION;
	case X_D3DTS_TEXTURE0:
		return D3DTS_TEXTURE0;
	case X_D3DTS_TEXTURE1:
		return D3DTS_TEXTURE1;
	case X_D3DTS_TEXTURE2:
		return D3DTS_TEXTURE2;
	case X_D3DTS_TEXTURE3:
		return D3DTS_TEXTURE3;
	case X_D3DTS_WORLD:
		return D3DTS_WORLDMATRIX(0);
	case X_D3DTS_WORLD1:
		return D3DTS_WORLDMATRIX(1);
	case X_D3DTS_WORLD2:
		return D3DTS_WORLDMATRIX(2);
	case X_D3DTS_WORLD3:
		return D3DTS_WORLDMATRIX(3);
	case X_D3DTS_MAX:
		EmuWarning("Ignoring X_D3DTRANSFORMSTATETYPE : X_D3DTS_MAX");
		return (D3DTRANSFORMSTATETYPE)0;
	default:
		CxbxKrnlCleanup("Unknown X_D3DTRANSFORMSTATETYPE (0x%.08X)", (DWORD)Value);
		return (D3DTRANSFORMSTATETYPE)Value; // Never reached
	}
}

// convert from xbox to pc texture stage state
D3DSAMPLERSTATETYPE EmuXB2PC_D3DTSS(X_D3DTEXTURESTAGESTATETYPE Value)
{
	if (Value <= X_D3DTSS_LAST)
		return DxbxTextureStageStateInfo[Value].PC;
	else
		return D3DSAMP_UNSUPPORTED;
}

#if 0
// convert from pc to xbox texture transform state types (unnecessary so far)
X_D3DTRANSFORMSTATETYPE EmuPC2XB_D3DTSS(D3DTRANSFORMSTATETYPE State)
{
	if (Value <= X_D3DTSS_LAST)
		return DxbxTextureStageStateInfo[Value].PC;

	return D3DSAMP_UNSUPPORTED;
	
	if ((uint32)State < 4)
		return (D3DTRANSFORMSTATETYPE)(State - 2);

	if ((uint32)State < 20)
		return (D3DTRANSFORMSTATETYPE)(State - 14);

	if ((uint32)State > 255)
		return (D3DTRANSFORMSTATETYPE)(State - 250);

	CxbxKrnlCleanup("Unknown D3DTRANSFORMSTATETYPE (0x%.08X)", (DWORD)State);
	return State; // Never reached
}
#endif

// convert from xbox to pc blend ops
D3DBLENDOP EmuXB2PC_D3DBLENDOP(X_D3DBLENDOP Value)
{
	switch (Value) {
	case X_D3DBLENDOP_ADD:
		return D3DBLENDOP_ADD;
	case X_D3DBLENDOP_SUBTRACT:
		return D3DBLENDOP_SUBTRACT;
	case X_D3DBLENDOP_REVSUBTRACT:
		return D3DBLENDOP_REVSUBTRACT;
	case X_D3DBLENDOP_MIN:
		return D3DBLENDOP_MIN;
	case X_D3DBLENDOP_MAX:
		return D3DBLENDOP_MAX;
	case X_D3DBLENDOP_ADDSIGNED:
		EmuWarning("Unsupported X_D3DBLENDOP : X_D3DBLENDOP_ADDSIGNED. Using D3DBLENDOP_ADD approximation.");
		return D3DBLENDOP_ADD;
	case X_D3DBLENDOP_REVSUBTRACTSIGNED:
		EmuWarning("Unsupported X_D3DBLENDOP : X_D3DBLENDOP_REVSUBTRACTSIGNED. Using D3DBLENDOP_REVSUBTRACT approximation.");
		return D3DBLENDOP_REVSUBTRACT;
	default:
		CxbxKrnlCleanup("Unknown X_D3DBLENDOP (0x%.08X)", (DWORD)Value);
		return (D3DBLENDOP)Value; // Never reached
	}
}

// convert from xbox to pc blend types 
D3DBLEND EmuXB2PC_D3DBLEND(X_D3DBLEND Value)
{
	switch (Value) {
	case X_D3DBLEND_ZERO               : return D3DBLEND_ZERO;
	case X_D3DBLEND_ONE                : return D3DBLEND_ONE;
	case X_D3DBLEND_SRCCOLOR           : return D3DBLEND_SRCCOLOR;
	case X_D3DBLEND_INVSRCCOLOR        : return D3DBLEND_INVSRCCOLOR;
	case X_D3DBLEND_SRCALPHA           : return D3DBLEND_SRCALPHA;
	case X_D3DBLEND_INVSRCALPHA        : return D3DBLEND_INVSRCALPHA;
	case X_D3DBLEND_DESTALPHA          : return D3DBLEND_DESTALPHA;
	case X_D3DBLEND_INVDESTALPHA       : return D3DBLEND_INVDESTALPHA;
	case X_D3DBLEND_DESTCOLOR          : return D3DBLEND_DESTCOLOR;
	case X_D3DBLEND_INVDESTCOLOR       : return D3DBLEND_INVDESTCOLOR;
	case X_D3DBLEND_SRCALPHASAT        : return D3DBLEND_SRCALPHASAT;
#ifdef DXBX_USE_D3D9
	// Xbox extensions not supported by D3D8, but available in D3D9 :
	case X_D3DBLEND_CONSTANTCOLOR      : return D3DBLEND_BLENDFACTOR;
	case X_D3DBLEND_INVCONSTANTCOLOR   : return D3DBLEND_INVBLENDFACTOR;
#endif
	default:
		// Xbox extensions that have to be approximated :
		switch (Value) {
#ifndef DXBX_USE_D3D9
		// Not supported by D3D8 :
		case X_D3DBLEND_CONSTANTCOLOR   : return D3DBLEND_SRCCOLOR;
		case X_D3DBLEND_INVCONSTANTCOLOR: return D3DBLEND_INVSRCCOLOR;
#endif
		// Not supported in D3D8 and D3D9 :
		case X_D3DBLEND_CONSTANTALPHA   : return D3DBLEND_SRCALPHA;
		case X_D3DBLEND_INVCONSTANTALPHA: return D3DBLEND_INVSRCALPHA;
		// Note : Xbox doesn't support D3DBLEND_BOTHSRCALPHA and D3DBLEND_BOTHINVSRCALPHA
		default:
			CxbxKrnlCleanup("Unknown X_D3DBLEND (0x%.08X)", (DWORD)Value);
			return (D3DBLEND)Value; // Never reached
		}

		EmuWarning("Unsupported X_D3DBLEND (0x%.08X). Using D3DBLEND_ONE approximation.", (DWORD)Value);
		return D3DBLEND_ONE;
	}
}

// convert from xbox to pc comparison functions
D3DCMPFUNC EmuXB2PC_D3DCMPFUNC(X_D3DCMPFUNC Value)
{
	// Was : (D3DCMPFUNC)(((DWORD)Value & 0xF) + 1);
	switch (Value) {
	case X_D3DCMP_NEVER:
		return D3DCMP_NEVER;
	case X_D3DCMP_LESS:
		return D3DCMP_LESS;
	case X_D3DCMP_EQUAL:
		return D3DCMP_EQUAL;
	case X_D3DCMP_LESSEQUAL:
		return D3DCMP_LESSEQUAL;
	case X_D3DCMP_GREATER:
		return D3DCMP_GREATER;
	case X_D3DCMP_NOTEQUAL:
		return D3DCMP_NOTEQUAL;
	case X_D3DCMP_GREATEREQUAL:
		return D3DCMP_GREATEREQUAL;
	case X_D3DCMP_ALWAYS:
		return D3DCMP_ALWAYS;
	default:
		if (Value == 0)
			EmuWarning("X_D3DCMPFUNC 0 is unsupported");
		else
			CxbxKrnlCleanup("Unknown X_D3DCMPFUNC (0x%.08X)", (DWORD)Value);

		return (D3DCMPFUNC)Value;
	}
}

// convert from xbox to pc fill modes
D3DFILLMODE EmuXB2PC_D3DFILLMODE(X_D3DFILLMODE Value)
{
	// was return (D3DFILLMODE)((Value & 0xF) + 1);
	switch (Value) {
	case X_D3DFILL_POINT:
		return D3DFILL_POINT;
	case X_D3DFILL_WIREFRAME:
		return D3DFILL_WIREFRAME;
	case X_D3DFILL_SOLID:
		return D3DFILL_SOLID;
	default:
		if (Value == 0)
			EmuWarning("X_D3DFILLMODE 0 is unsupported");
		else
			CxbxKrnlCleanup("Unknown X_D3DFILLMODE (0x%.08X)", (DWORD)Value);

		return (D3DFILLMODE)Value;
	}
}

// convert from xbox to pc shade modes
D3DSHADEMODE EmuXB2PC_D3DSHADEMODE(X_D3DSHADEMODE Value)
{
	// Was :  (D3DSHADEMODE)(((DWORD)Value & 0x3) + 1);
	switch (Value) {
	case X_D3DSHADE_FLAT:
		return D3DSHADE_FLAT;
	case X_D3DSHADE_GOURAUD:
		return D3DSHADE_GOURAUD;
	default:
		if (Value == 0)
			EmuWarning("X_D3DSHADEMODE 0 is unsupported");
		else
			CxbxKrnlCleanup("Unknown X_D3DSHADEMODE (0x%.08X)", (DWORD)Value);

		return (D3DSHADEMODE)Value;
	}
}

// convert from xbox to pc stencilop modes
D3DSTENCILOP EmuXB2PC_D3DSTENCILOP(X_D3DSTENCILOP Value)
{
	switch (Value) {
	case X_D3DSTENCILOP_ZERO:
		return D3DSTENCILOP_ZERO;
	case X_D3DSTENCILOP_KEEP:
		return D3DSTENCILOP_KEEP;
	case X_D3DSTENCILOP_REPLACE:
		return D3DSTENCILOP_REPLACE;
	case X_D3DSTENCILOP_INCRSAT:
		return D3DSTENCILOP_INCRSAT;
	case X_D3DSTENCILOP_DECRSAT:
		return D3DSTENCILOP_DECRSAT;
	case X_D3DSTENCILOP_INVERT:
		return D3DSTENCILOP_INVERT;
	case X_D3DSTENCILOP_INCR:
		return D3DSTENCILOP_INCR;
	case X_D3DSTENCILOP_DECR:
		return D3DSTENCILOP_DECR;
	default:
		CxbxKrnlCleanup("Unknown X_D3DSTENCILOP (0x%.08X)", (DWORD)Value);
		return (D3DSTENCILOP)Value; // Never reached
	}
}

DWORD EmuXB2PC_D3DTEXTUREADDRESS(DWORD Value)
{
	switch (Value) {
	case X_D3DTADDRESS_WRAP:
		return D3DTADDRESS_WRAP;
	case X_D3DTADDRESS_MIRROR:
		return D3DTADDRESS_MIRROR;
	case X_D3DTADDRESS_CLAMP:
		return D3DTADDRESS_CLAMP;
	case X_D3DTADDRESS_BORDER:
		return D3DTADDRESS_BORDER;
	case X_D3DTADDRESS_CLAMPTOEDGE:
		// Note : PC has D3DTADDRESS_MIRRORONCE in it's place
		EmuWarning("Unsupported X_D3DTEXTUREADDRESS : X_D3DTADDRESS_CLAMPTOEDGE. Using D3DTADDRESS_BORDER approximation.");
		return D3DTADDRESS_BORDER; // Note : D3DTADDRESS_CLAMP is not a good approximation
	default:
		if (Value == 0)
			EmuWarning("X_D3DTEXTUREADDRESS 0 is unsupported");
		else
			CxbxKrnlCleanup("Unknown X_D3DTEXTUREADDRESS (0x%.08X)", (DWORD)Value);

		return (DWORD)Value;
	}
}

DWORD EmuXB2PC_D3DTEXTUREFILTERTYPE(DWORD Value)
{
	switch (Value) {
	case X_D3DTEXF_NONE:
		return D3DTEXF_NONE;
	case X_D3DTEXF_POINT:
		return D3DTEXF_POINT;
	case X_D3DTEXF_LINEAR:
		return D3DTEXF_LINEAR;
	case X_D3DTEXF_ANISOTROPIC:
		return D3DTEXF_ANISOTROPIC;
	case X_D3DTEXF_QUINCUNX:
		// Note : PC has D3DTEXF_FLATCUBIC in it's place
		CxbxKrnlCleanup("X_D3DTEXF_QUINCUNX is unsupported (temporarily)");
		return (DWORD)0; // Never reached
	case X_D3DTEXF_GAUSSIANCUBIC:
		return D3DTEXF_GAUSSIANCUBIC;
	default:
		CxbxKrnlCleanup("Unknown X_D3DTEXTUREFILTERTYPE (0x%.08X)", (DWORD)Value);
		return (DWORD)0; // Never reached
	}
}

// convert from Xbox direct3d to PC direct3d enumeration
D3DVERTEXBLENDFLAGS EmuXB2PC_D3DVERTEXBLENDFLAGS(X_D3DVERTEXBLENDFLAGS Value)
{
	// convert from Xbox direct3d to PC direct3d enumeration
	switch (Value) {
	case X_D3DVBF_DISABLE: return D3DVBF_DISABLE;
	case X_D3DVBF_1WEIGHTS: return D3DVBF_1WEIGHTS;
	case X_D3DVBF_2WEIGHTS: return D3DVBF_2WEIGHTS;
	case X_D3DVBF_3WEIGHTS: return D3DVBF_3WEIGHTS;
		/* Xbox only :
			case X_D3DVBF_2WEIGHTS2MATRICES : return;
			case X_D3DVBF_3WEIGHTS3MATRICES : return;
			case X_D3DVBF_4WEIGHTS4MATRICES : Result := ;
		   Xbox doesn't support :
			D3DVBF_TWEENING = 255,
			D3DVBF_0WEIGHTS = 256
		*/
	default:
		CxbxKrnlCleanup("Unsupported X_D3DVERTEXBLENDFLAGS (0x%.08X)", (DWORD)Value);
		return (D3DVERTEXBLENDFLAGS)Value; // Never reached
	}
}

DWORD EmuXB2PC_D3DCOLORWRITEENABLE(X_D3DCOLORWRITEENABLE Value)
{
	DWORD Result = 0;
	if (Value & X_D3DCOLORWRITEENABLE_RED)
		Result |= D3DCOLORWRITEENABLE_RED;
	if (Value & X_D3DCOLORWRITEENABLE_GREEN)
		Result |= D3DCOLORWRITEENABLE_GREEN;
	if (Value & X_D3DCOLORWRITEENABLE_BLUE)
		Result |= D3DCOLORWRITEENABLE_BLUE;
	if (Value & X_D3DCOLORWRITEENABLE_ALPHA)
		Result |= D3DCOLORWRITEENABLE_ALPHA;
	return Result;
}

D3DTEXTUREOP EmuXB2PC_D3DTEXTUREOP(X_D3DTEXTUREOP Value)
{
	switch (Value) {
	case X_D3DTOP_DISABLE: return D3DTOP_DISABLE;
	case X_D3DTOP_SELECTARG1: return D3DTOP_SELECTARG1;
	case X_D3DTOP_SELECTARG2: return D3DTOP_SELECTARG2;
	case X_D3DTOP_MODULATE: return D3DTOP_MODULATE;
	case X_D3DTOP_MODULATE2X: return D3DTOP_MODULATE2X;
	case X_D3DTOP_MODULATE4X: return D3DTOP_MODULATE4X;
	case X_D3DTOP_ADD: return D3DTOP_ADD;
	case X_D3DTOP_ADDSIGNED: return D3DTOP_ADDSIGNED;
	case X_D3DTOP_ADDSIGNED2X: return D3DTOP_ADDSIGNED2X;
	case X_D3DTOP_SUBTRACT: return D3DTOP_SUBTRACT;
	case X_D3DTOP_ADDSMOOTH: return D3DTOP_ADDSMOOTH;

	// Linear alpha blend: Arg1*(Alpha) + Arg2*(1-Alpha)
	case X_D3DTOP_BLENDDIFFUSEALPHA: return D3DTOP_BLENDDIFFUSEALPHA;// iterated alpha
	case X_D3DTOP_BLENDTEXTUREALPHA: return D3DTOP_BLENDTEXTUREALPHA; // texture alpha
	case X_D3DTOP_BLENDFACTORALPHA: return D3DTOP_BLENDFACTORALPHA; // alpha from D3DRS_TEXTUREFACTOR
	// Linear alpha blend with pre-multiplied arg1 input: Arg1 + Arg2*(1-Alpha)
	case X_D3DTOP_BLENDCURRENTALPHA: return D3DTOP_BLENDCURRENTALPHA; // by alpha of current color
	case X_D3DTOP_BLENDTEXTUREALPHAPM: return D3DTOP_BLENDTEXTUREALPHAPM; // texture alpha

	case X_D3DTOP_PREMODULATE: return D3DTOP_PREMODULATE;
	case X_D3DTOP_MODULATEALPHA_ADDCOLOR: return D3DTOP_MODULATEALPHA_ADDCOLOR;
	case X_D3DTOP_MODULATECOLOR_ADDALPHA: return D3DTOP_MODULATECOLOR_ADDALPHA;
	case X_D3DTOP_MODULATEINVALPHA_ADDCOLOR: return D3DTOP_MODULATEINVALPHA_ADDCOLOR;
	case X_D3DTOP_MODULATEINVCOLOR_ADDALPHA: return D3DTOP_MODULATEINVCOLOR_ADDALPHA;
	case X_D3DTOP_DOTPRODUCT3: return D3DTOP_DOTPRODUCT3;
	case X_D3DTOP_MULTIPLYADD: return D3DTOP_MULTIPLYADD;
	case X_D3DTOP_LERP: return D3DTOP_LERP;
	case X_D3DTOP_BUMPENVMAP: return D3DTOP_BUMPENVMAP;
	case X_D3DTOP_BUMPENVMAPLUMINANCE: return D3DTOP_BUMPENVMAPLUMINANCE;
	default:
		CxbxKrnlCleanup("Unsupported X_D3DTEXTUREOP (0x%.08X)", (DWORD)Value);
		return (D3DTEXTUREOP)Value; // Never reached
	}
}

DWORD EmuXB2PC_D3DCLEAR_FLAGS(DWORD Value)
{
	DWORD Result = 0;
	// Dxbx note : Xbox can clear A,R,G and B independently, but PC has to clear them all :
	if (Value & X_D3DCLEAR_TARGET) Result |= D3DCLEAR_TARGET;
	if (Value & X_D3DCLEAR_ZBUFFER) Result |= D3DCLEAR_ZBUFFER;
	if (Value & X_D3DCLEAR_STENCIL) Result |= D3DCLEAR_STENCIL;
	return Result;
}

DWORD EmuXB2PC_D3DWRAP(DWORD Value)
{
	DWORD Result = 0;
	if (Value & X_D3DWRAP_U) Result |= D3DWRAP_U;
	if (Value & X_D3DWRAP_V) Result |= D3DWRAP_V;
	if (Value & X_D3DWRAP_W) Result |= D3DWRAP_W;
	return Result;
}

// convert from Xbox D3D to PC D3D enumeration
D3DCULL EmuXB2PC_D3DCULL(X_D3DCULL Value)
// TODO: XDK-Specific Tables? So far they are the same
{
	switch (Value) {
	case X_D3DCULL_NONE:
		return D3DCULL_NONE;
	case X_D3DCULL_CW:
		return D3DCULL_CW;
	case X_D3DCULL_CCW:
		return D3DCULL_CCW;
	default:
		CxbxKrnlCleanup("Unknown X_D3DCULL (0x%.08X)", (DWORD)Value);
		return (D3DCULL)Value; // Never reached
	}
}

// convert xbox->pc primitive type
D3DPRIMITIVETYPE EmuXB2PC_D3DPrimitiveType(X_D3DPRIMITIVETYPE Value)
{
	switch (Value) {
	case X_D3DPT_NONE: return (D3DPRIMITIVETYPE)0; // Dxbx addition
	case X_D3DPT_POINTLIST: return D3DPT_POINTLIST;
	case X_D3DPT_LINELIST: return D3DPT_LINELIST;
	case X_D3DPT_LINELOOP: return D3DPT_LINESTRIP; // Xbox
	case X_D3DPT_LINESTRIP: return D3DPT_LINESTRIP;
	case X_D3DPT_TRIANGLELIST: return D3DPT_TRIANGLELIST;
	case X_D3DPT_TRIANGLESTRIP: return D3DPT_TRIANGLESTRIP;
	case X_D3DPT_TRIANGLEFAN: return D3DPT_TRIANGLEFAN;
	case X_D3DPT_QUADLIST: return D3DPT_TRIANGLELIST; // Xbox
	case X_D3DPT_QUADSTRIP: return D3DPT_TRIANGLESTRIP; // Xbox
	case X_D3DPT_POLYGON: return D3DPT_TRIANGLEFAN; // Xbox
	case X_D3DPT_INVALID: return D3DPT_FORCE_DWORD; // Cxbx addition
	default:
		CxbxKrnlCleanup("Unknown X_D3DPRIMITIVETYPE (0x%.08X)", (DWORD)Value);
		return (D3DPRIMITIVETYPE)Value; // Never reached
	}
}

// SetTextureState_TexCoordIndex
DWORD EmuXB2PC_D3DTSS_TCI(DWORD Value)
{
	// Native doesn't support D3DTSS_TCI_OBJECT, D3DTSS_TCI_SPHERE, D3DTSS_TCI_TEXGEN_MAX or higher:
	if ((Value & 0xFFFF0000) > D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR) // Dxbx note : Cxbx uses 0x00030000, which is not enough for the Strip XDK sample!
		EmuWarning("EmuD3DDevice_SetTextureState_TexCoordIndex: Unknown TexCoordIndex Value (0x%.08X)", Value);

	// BUG FIX: The lower 16 bits were causing false Unknown TexCoordIndex errors.
	// Check for 0x00040000 instead.

	if (Value >= 0x00040000)
		CxbxKrnlCleanup("EmuD3DDevice_SetTextureState_TexCoordIndex: Unknown TexCoordIndex Value (0x%.08X)", Value);

	return Value & 0xFFFF0000;
}

void XTL::EmuUnswizzleRect
(
	PVOID pSrcBuff,
	DWORD dwWidth,
	DWORD dwHeight,
	DWORD dwDepth,
	PVOID pDstBuff,
	DWORD dwDestPitch,
	DWORD dwBPP // expressed in Bytes Per Pixel
) // Source : Dxbx
{
	// TODO : The following could be done using a lookup table :
	DWORD dwMaskX = 0, dwMaskY = 0, dwMaskZ = 0;
	for (uint i = 1, j = 1; (i <= dwWidth) || (i <= dwHeight) || (i <= dwDepth); i <<= 1) {
		if (i < dwWidth) {
			dwMaskX = dwMaskX | j;
			j <<= 1;
		};

		if (i < dwHeight) {
			dwMaskY = dwMaskY | j;
			j <<= 1;
		}

		if (i < dwDepth) {
			dwMaskZ = dwMaskZ | j;
			j <<= 1;
		}
	}

	// get the biggest mask
	DWORD dwMaskMax;
	if (dwMaskX > dwMaskY)
		dwMaskMax = dwMaskX;
	else
		dwMaskMax = dwMaskY;

	if (dwMaskZ > dwMaskMax)
		dwMaskMax = dwMaskZ;

	DWORD dwStartX = 0, dwOffsetX = 0;
	DWORD dwStartY = 0, dwOffsetY = 0;
	DWORD dwStartZ = 0, dwOffsetW = 0;
	/* TODO : Use values from poDst and rSrc to initialize above values, after which the following makes more sense:
	for (uint i=1; i <= dwMaskMax; i <<= 1) {
	if (i <= dwMaskX) {
	if (dwMaskX & i)
	dwStartX |= (dwOffsetX & i);
	else
	dwOffsetX <<= 1;
	}

	if (i <= dwMaskY) {
	if (dwMaskY & i)
	dwStartY |= dwOffsetY & i;
	else
	dwOffsetY <<= 1;
	}

	if (i <= dwMaskZ) {
	if (dwMaskZ & i)
	dwStartZ |= dwOffsetZ & i;
	else
	dwOffsetZ <<= 1;
	}
	}*/

	DWORD dwZ = dwStartZ;
	for (uint z = 0; z < dwDepth; z++) {
		DWORD dwY = dwStartY;
		for (uint y = 0; y < dwHeight; y++) {
			DWORD dwX = dwStartX;
			for (uint x = 0; x < dwWidth; x++) {
				int SrcOffset = ((dwX | dwY | dwZ) * dwBPP);
				memcpy(pDstBuff, (PBYTE)pSrcBuff + SrcOffset, dwBPP); // copy one pixel
				pDstBuff = (PBYTE)pDstBuff + dwBPP; // Step to next pixel in destination
				dwX = (dwX - dwMaskX) & dwMaskX; // step to next pixel in source
			}

			pDstBuff = (PBYTE)pDstBuff + dwDestPitch - (dwWidth * dwBPP); // step to next line in destination
			dwY = (dwY - dwMaskY) & dwMaskY; // step to next line in source
		}

		// TODO : How to step to next level in destination? Should X and Y be recalculated per level?
		dwZ = (dwZ - dwMaskZ) & dwMaskZ; // step to next level in source
	}
} // EmuUnswizzleRect NOPATCH

// Convert a 'method' DWORD into it's associated 'pixel-shader' or 'simple' render state.
XTL::X_D3DRENDERSTATETYPE XTL::DxbxXboxMethodToRenderState(const NV2AMETHOD aMethod)
{
	// TODO : The list below is incomplete - use DxbxRenderStateInfo to complete this.

	// Dxbx note : Let the compiler sort this out, should be much quicker :
	switch (aMethod & NV2A_METHOD_MASK)
	{
	// case /*0x00000100*/NV2A_NOP: return X_D3DRS_PS_RESERVED; // XDK 3424 uses 0x00000100 (NOP), while 3911 onwards uses 0x00001d90 (SET_COLOR_CLEAR_VALUE)
	// Actually, NV2A_NOP is (ab)used as a callback mechanism by InsertCallback, with one argument :
	// (DWORD)6 for read callbacks, (DWORD)7 for write callbacks.
	// The NV2A_NOP method triggers a signal on these non-null arguments, and handles the type specified in the argument:
	// Read and write callbacks are handled by retreiving the callback itself from 0x00001d8c (NV2A_CLEAR_DEPTH_VALUE)
	// and it's context data address from 0x00001d90 (NV2A_CLEAR_VALUE), after which the callback is executed
	case /*0x000002A4*/NV2A_FOG_ENABLE: return X_D3DRS_FOGENABLE;
	case /*0x00000260*/NV2A_RC_IN_ALPHA(0): return X_D3DRS_PSALPHAINPUTS0;
	case /*0x00000264*/NV2A_RC_IN_ALPHA(1): return X_D3DRS_PSALPHAINPUTS1;
	case /*0x00000268*/NV2A_RC_IN_ALPHA(2): return X_D3DRS_PSALPHAINPUTS2;
	case /*0x0000026c*/NV2A_RC_IN_ALPHA(3): return X_D3DRS_PSALPHAINPUTS3;
	case /*0x00000270*/NV2A_RC_IN_ALPHA(4): return X_D3DRS_PSALPHAINPUTS4;
	case /*0x00000274*/NV2A_RC_IN_ALPHA(5): return X_D3DRS_PSALPHAINPUTS5;
	case /*0x00000278*/NV2A_RC_IN_ALPHA(6): return X_D3DRS_PSALPHAINPUTS6;
	case /*0x0000027c*/NV2A_RC_IN_ALPHA(7): return X_D3DRS_PSALPHAINPUTS7;
	case /*0x00000288*/NV2A_RC_FINAL0: return X_D3DRS_PSFINALCOMBINERINPUTSABCD;
	case /*0x0000028c*/NV2A_RC_FINAL1: return X_D3DRS_PSFINALCOMBINERINPUTSEFG;
	case /*0x00000294*/NV2A_LIGHT_MODEL: return X_D3DRS_LIGHTING; // TODO : Used in combination with NV2A_LIGHTING_ENABLE?
	case /*0x00000300*/NV2A_ALPHA_FUNC_ENABLE: return X_D3DRS_ALPHATESTENABLE;
	case /*0x00000304*/NV2A_BLEND_FUNC_ENABLE: return X_D3DRS_ALPHABLENDENABLE;
	case /*0x0000030C*/NV2A_DEPTH_TEST_ENABLE: return X_D3DRS_ZENABLE;
	case /*0x00000310*/NV2A_DITHER_ENABLE: return X_D3DRS_DITHERENABLE;
	case /*0x00000314*/NV2A_LIGHTING_ENABLE: return X_D3DRS_LIGHTING; // TODO : Used in combination with NV2A_LIGHT_MODEL?
	case /*0x00000318*/NV2A_POINT_PARAMETERS_ENABLE: return X_D3DRS_POINTSCALEENABLE;
	case /*0x0000031C*/NV2A_POINT_SMOOTH_ENABLE: return X_D3DRS_POINTSPRITEENABLE;
	case /*0x00000328*/NV2A_SKIN_MODE: return X_D3DRS_VERTEXBLEND;
	case /*0x0000032C*/NV2A_STENCIL_ENABLE: return X_D3DRS_STENCILENABLE;
	case /*0x00000330*/NV2A_POLYGON_OFFSET_POINT_ENABLE: return X_D3DRS_POINTOFFSETENABLE;
	case /*0x00000334*/NV2A_POLYGON_OFFSET_LINE_ENABLE: return X_D3DRS_WIREFRAMEOFFSETENABLE;
	case /*0x00000338*/NV2A_POLYGON_OFFSET_FILL_ENABLE: return X_D3DRS_SOLIDOFFSETENABLE;
	case /*0x0000033c*/NV2A_ALPHA_FUNC_FUNC: return X_D3DRS_ALPHAFUNC;
	case /*0x00000340*/NV2A_ALPHA_FUNC_REF: return X_D3DRS_ALPHAREF;
	case /*0x00000344*/NV2A_BLEND_FUNC_SRC: return X_D3DRS_SRCBLEND;
	case /*0x00000348*/NV2A_BLEND_FUNC_DST: return X_D3DRS_DESTBLEND;
	case /*0x0000034c*/NV2A_BLEND_COLOR: return X_D3DRS_BLENDCOLOR;
	case /*0x00000350*/NV2A_BLEND_EQUATION: return X_D3DRS_BLENDOP;
	case /*0x00000354*/NV2A_DEPTH_FUNC: return X_D3DRS_ZFUNC;
	case /*0x00000358*/NV2A_COLOR_MASK: return X_D3DRS_COLORWRITEENABLE;
	case /*0x0000035c*/NV2A_DEPTH_WRITE_ENABLE: return X_D3DRS_ZWRITEENABLE;
	case /*0x00000360*/NV2A_STENCIL_MASK: return X_D3DRS_STENCILWRITEMASK;
	case /*0x00000364*/NV2A_STENCIL_FUNC_FUNC: return X_D3DRS_STENCILFUNC;
	case /*0x00000368*/NV2A_STENCIL_FUNC_REF: return X_D3DRS_STENCILREF;
	case /*0x0000036c*/NV2A_STENCIL_FUNC_MASK: return X_D3DRS_STENCILMASK;
	case /*0x00000374*/NV2A_STENCIL_OP_ZFAIL: return X_D3DRS_STENCILZFAIL;
	case /*0x00000378*/NV2A_STENCIL_OP_ZPASS: return X_D3DRS_STENCILPASS;
	case /*0x0000037c*/NV2A_SHADE_MODEL: return X_D3DRS_SHADEMODE;
	case /*0x00000380*/NV2A_LINE_WIDTH: return X_D3DRS_LINEWIDTH;
	case /*0x00000384*/NV2A_POLYGON_OFFSET_FACTOR: return X_D3DRS_POLYGONOFFSETZSLOPESCALE;
	case /*0x00000388*/NV2A_POLYGON_OFFSET_UNITS: return X_D3DRS_POLYGONOFFSETZOFFSET;
	case /*0x0000038c*/NV2A_POLYGON_MODE_FRONT: return X_D3DRS_FILLMODE;
	case /*0x000003A0*/NV2A_FRONT_FACE: return X_D3DRS_FRONTFACE;
	case /*0x000003A4*/NV2A_NORMALIZE_ENABLE: return X_D3DRS_NORMALIZENORMALS;
	case /*0x000009f8*/NV2A_SWATH_WIDTH: return X_D3DRS_SWATHWIDTH;
	case /*0x00000a60*/NV2A_RC_CONSTANT_COLOR0(0): return X_D3DRS_PSCONSTANT0_0;
	case /*0x00000a64*/NV2A_RC_CONSTANT_COLOR0(1): return X_D3DRS_PSCONSTANT0_1;
	case /*0x00000a68*/NV2A_RC_CONSTANT_COLOR0(2): return X_D3DRS_PSCONSTANT0_2;
	case /*0x00000a6c*/NV2A_RC_CONSTANT_COLOR0(3): return X_D3DRS_PSCONSTANT0_3;
	case /*0x00000a70*/NV2A_RC_CONSTANT_COLOR0(4): return X_D3DRS_PSCONSTANT0_4;
	case /*0x00000a74*/NV2A_RC_CONSTANT_COLOR0(5): return X_D3DRS_PSCONSTANT0_5;
	case /*0x00000a78*/NV2A_RC_CONSTANT_COLOR0(6): return X_D3DRS_PSCONSTANT0_6;
	case /*0x00000a7c*/NV2A_RC_CONSTANT_COLOR0(7): return X_D3DRS_PSCONSTANT0_7;
	case /*0x00000a80*/NV2A_RC_CONSTANT_COLOR1(0): return X_D3DRS_PSCONSTANT1_0;
	case /*0x00000a84*/NV2A_RC_CONSTANT_COLOR1(1): return X_D3DRS_PSCONSTANT1_1;
	case /*0x00000a88*/NV2A_RC_CONSTANT_COLOR1(2): return X_D3DRS_PSCONSTANT1_2;
	case /*0x00000a8c*/NV2A_RC_CONSTANT_COLOR1(3): return X_D3DRS_PSCONSTANT1_3;
	case /*0x00000a90*/NV2A_RC_CONSTANT_COLOR1(4): return X_D3DRS_PSCONSTANT1_4;
	case /*0x00000a94*/NV2A_RC_CONSTANT_COLOR1(5): return X_D3DRS_PSCONSTANT1_5;
	case /*0x00000a98*/NV2A_RC_CONSTANT_COLOR1(6): return X_D3DRS_PSCONSTANT1_6;
	case /*0x00000a9c*/NV2A_RC_CONSTANT_COLOR1(7): return X_D3DRS_PSCONSTANT1_7;
	case /*0x00000aa0*/NV2A_RC_OUT_ALPHA(0): return X_D3DRS_PSALPHAOUTPUTS0;
	case /*0x00000aa4*/NV2A_RC_OUT_ALPHA(1): return X_D3DRS_PSALPHAOUTPUTS1;
	case /*0x00000aa8*/NV2A_RC_OUT_ALPHA(2): return X_D3DRS_PSALPHAOUTPUTS2;
	case /*0x00000aac*/NV2A_RC_OUT_ALPHA(3): return X_D3DRS_PSALPHAOUTPUTS3;
	case /*0x00000ab0*/NV2A_RC_OUT_ALPHA(4): return X_D3DRS_PSALPHAOUTPUTS4;
	case /*0x00000ab4*/NV2A_RC_OUT_ALPHA(5): return X_D3DRS_PSALPHAOUTPUTS5;
	case /*0x00000ab8*/NV2A_RC_OUT_ALPHA(6): return X_D3DRS_PSALPHAOUTPUTS6;
	case /*0x00000abc*/NV2A_RC_OUT_ALPHA(7): return X_D3DRS_PSALPHAOUTPUTS7;
	case /*0x00000ac0*/NV2A_RC_IN_RGB(0): return X_D3DRS_PSRGBINPUTS0;
	case /*0x00000ac4*/NV2A_RC_IN_RGB(1): return X_D3DRS_PSRGBINPUTS1;
	case /*0x00000ac8*/NV2A_RC_IN_RGB(2): return X_D3DRS_PSRGBINPUTS2;
	case /*0x00000acc*/NV2A_RC_IN_RGB(3): return X_D3DRS_PSRGBINPUTS3;
	case /*0x00000ad0*/NV2A_RC_IN_RGB(4): return X_D3DRS_PSRGBINPUTS4;
	case /*0x00000ad4*/NV2A_RC_IN_RGB(5): return X_D3DRS_PSRGBINPUTS5;
	case /*0x00000ad8*/NV2A_RC_IN_RGB(6): return X_D3DRS_PSRGBINPUTS6;
	case /*0x00000adc*/NV2A_RC_IN_RGB(7): return X_D3DRS_PSRGBINPUTS7;
	case /*0x0000147c*/NV2A_POLYGON_STIPPLE_ENABLE: return X_D3DRS_STIPPLEENABLE;
	case /*0x000017f8*/NV2A_TX_SHADER_CULL_MODE: return X_D3DRS_PSCOMPAREMODE;
	case /*0x00001d78*/NV2A_DEPTHCLIPCONTROL: return X_D3DRS_DEPTHCLIPCONTROL;
	case /*0x00001d7c*/NV2A_MULTISAMPLE_CONTROL: return X_D3DRS_MULTISAMPLEANTIALIAS; // Also send by X_D3DRS_MULTISAMPLEMASK (both values in 1 command)
	case /*0x00001d84*/NV2A_OCCLUDE_ZSTENCIL_EN: return X_D3DRS_OCCLUSIONCULLENABLE;
	// case /*0x00001d90*/NV2A_CLEAR_VALUE: Result := X_D3DRS_SIMPLE_UNUSED1; // TODO : Should this be X_D3DRS_PSTEXTUREMODES?!
	case /*0x00001e20*/NV2A_RC_COLOR0: return X_D3DRS_PSFINALCOMBINERCONSTANT0;
	case /*0x00001e24*/NV2A_RC_COLOR1: return X_D3DRS_PSFINALCOMBINERCONSTANT1;
	case /*0x00001e40*/NV2A_RC_OUT_RGB(0): return X_D3DRS_PSRGBOUTPUTS0;
	case /*0x00001e44*/NV2A_RC_OUT_RGB(1): return X_D3DRS_PSRGBOUTPUTS1;
	case /*0x00001e48*/NV2A_RC_OUT_RGB(2): return X_D3DRS_PSRGBOUTPUTS2;
	case /*0x00001e4c*/NV2A_RC_OUT_RGB(3): return X_D3DRS_PSRGBOUTPUTS3;
	case /*0x00001e50*/NV2A_RC_OUT_RGB(4): return X_D3DRS_PSRGBOUTPUTS4;
	case /*0x00001e54*/NV2A_RC_OUT_RGB(5): return X_D3DRS_PSRGBOUTPUTS5;
	case /*0x00001e58*/NV2A_RC_OUT_RGB(6): return X_D3DRS_PSRGBOUTPUTS6;
	case /*0x00001e5c*/NV2A_RC_OUT_RGB(7): return X_D3DRS_PSRGBOUTPUTS7;
	case /*0x00001e60*/NV2A_RC_ENABLE: return X_D3DRS_PSCOMBINERCOUNT;
	case /*0x00001e6c*/NV2A_TX_RCOMP: return X_D3DRS_SHADOWFUNC;
	case /*0x00001e74*/NV2A_TX_SHADER_DOTMAPPING: return X_D3DRS_PSDOTMAPPING;
	case /*0x00001e78*/NV2A_TX_SHADER_PREVIOUS: return X_D3DRS_PSINPUTTEXTURE;
	// Missing : 0x0000????: Result := X_D3DRS_PSTEXTUREMODES;
	default:
		return X_D3DRS_UNK; // Note : Dxbx returns ~0;
	}
}

}; // end of namespace XTL 

#if 0
/* Generic swizzle function, usable for both x and y dimensions.
When passing x, Max should be 2*height, and Shift should be 0
When passing y, Max should be width, and Shift should be 1 */
uint32 Swizzle(uint32 value, uint32 max, uint32 shift)
{
	uint32 result;

	if (value < max)
		result = value;
	else
		result = value % max;

	// The following is based on http://graphics.stanford.edu/~seander/bithacks.html#InterleaveBMN :
	// --------------------------------11111111111111111111111111111111
	result = (result | (result << 8)) & 0x00FF00FF; // 0000000000000000111111111111111100000000000000001111111111111111
	result = (result | (result << 4)) & 0x0F0F0F0F; // 0000111100001111000011110000111100001111000011110000111100001111
	result = (result | (result << 2)) & 0x33333333; // 0011001100110011001100110011001100110011001100110011001100110011
	result = (result | (result << 1)) & 0x55555555; // 0101010101010101010101010101010101010101010101010101010101010101

	result = result << shift; // y counts twice :      1010101010101010101010101010101010101010101010101010101010101010

	if (value >= max)
		result += (value / max) * max * max >> (1 - shift);  // x halves this

	return result;
}


typedef uint16 TRGB16;

using namespace XTL; // for X_D3DFMT_*

// test-case: Frogger, Turok, Crazy Taxi 3 and many more
bool WndMain::ReadS3TCFormatIntoBitmap(uint32 format, unsigned char *data, uint32 dataSize, int width, int height, int pitch, void*& bitmap)
{
	uint16 color[3];
	TRGB32 color32b[4];
	uint32 r, g, b, r1, g1, b1, pixelmap, j;
	int k, p, x, y;

	r = g = b = r1 = g1 = b1 = pixelmap = 0;
	j = k = p = x = y = 0;

	// sanity checks
	if (format != X_D3DFMT_DXT1 && format != X_D3DFMT_DXT3 && format != X_D3DFMT_DXT5)
		return false;
	if (!(width > 0) || !(height > 0))
		return false;

	while (j < dataSize) {

		if (format != X_D3DFMT_DXT1) // Skip X_D3DFMT_DXT3 and X_D3DFMT_DXT5 alpha data (ported from Dxbx)
			j += 8;

		// Read two 16-bit pixels
		color[0] = (data[j + 0] << 0) + (data[j + 1] << 8);
		color[1] = (data[j + 2] << 0) + (data[j + 3] << 8);

		// Read 5+6+5 bit color channels and convert them to 8+8+8 bit :
		r = ((color[0] >> 11) & 31) * 255 / 31;
		g = ((color[0] >> 5) & 63) * 255 / 63;
		b = ((color[0]) & 31) * 255 / 31;

		r1 = ((color[1] >> 11) & 31) * 255 / 31;
		g1 = ((color[1] >> 5) & 63) * 255 / 63;
		b1 = ((color[1]) & 31) * 255 / 31;

		// Build first half of RGB32 color map :
		color32b[0].R = (unsigned char)r;
		color32b[0].G = (unsigned char)g;
		color32b[0].B = (unsigned char)b;

		color32b[1].R = (unsigned char)r1;
		color32b[1].G = (unsigned char)g1;
		color32b[1].B = (unsigned char)b1;

		// Build second half of RGB32 color map :
		if (color[0] > color[1])
		{
			// Make up 2 new colors, 1/3 A + 2/3 B and 2/3 A + 1/3 B :
			color32b[2].R = (unsigned char)((r + r + r1 + 2) / 3);
			color32b[2].G = (unsigned char)((g + g + g1 + 2) / 3);
			color32b[2].B = (unsigned char)((b + b + b1 + 2) / 3);

			color32b[3].R = (unsigned char)((r + r1 + r1 + 2) / 3);
			color32b[3].G = (unsigned char)((g + g1 + g1 + 2) / 3);
			color32b[3].B = (unsigned char)((b + b1 + b1 + 2) / 3);
		}
		else
		{
			// Make up one new color : 1/2 A + 1/2 B :
			color32b[2].R = (unsigned char)((r + r1) / 2);
			color32b[2].G = (unsigned char)((g + g1) / 2);
			color32b[2].B = (unsigned char)((b + b1) / 2);

			color32b[3].R = (unsigned char)0;
			color32b[3].G = (unsigned char)0;
			color32b[3].B = (unsigned char)0;
		}

		x = (k / 2) % width;
		y = (k / 2) / width * 4;

		// Forza Motorsport needs this safety measure, as it has dataSize=147456, while we need 16384 bytes :
		if (y >= height)
			break;

		pixelmap = (data[j + 4] << 0)
			+ (data[j + 5] << 8)
			+ (data[j + 6] << 16)
			+ (data[j + 7] << 24);

		for (p = 0; p < 16; p++)
		{
			((TRGB32*)bitmap)[x + (p & 3) + (pitch / sizeof(TRGB32) * (y + (p >> 2)))] = color32b[pixelmap & 3];
			pixelmap >>= 2;
		};

		j += 8;
		k += 8;

	}
	return true;
}

// test-case: Baku Baku 2 (Homebrew)
bool WndMain::ReadSwizzledFormatIntoBitmap(uint32 format, unsigned char *data, uint32 dataSize, int width, int height, int pitch, void*& bitmap)
{
	uint32* xSwizzle;
	int x, y;
	uint32	ySwizzle;
	TRGB32* yscanline;

	// sanity checks
	if (format != X_D3DFMT_A8R8G8B8 && format != X_D3DFMT_X8R8G8B8)
		return false;
	if (!(width > 0) || !(height > 0))
		return false;

	xSwizzle = (uint32*)malloc(sizeof(uint32) * width);
	if (xSwizzle == NULL)
		return false;

	for (x = 0; x < width; x++)
		xSwizzle[x] = Swizzle(x, (height * 2), 0);

	// Loop over all lines :
	for (y = 0; y < height; y++)
	{
		// Calculate y-swizzle :
		ySwizzle = Swizzle(y, width, 1);

		// Copy whole line in one go (using pre-calculated x-swizzle) :
		yscanline = &((TRGB32*)bitmap)[y*(pitch / sizeof(TRGB32))];

		for (x = 0; x < width; x++)
			yscanline[x] = ((TRGB32*)data)[xSwizzle[x] + ySwizzle];
	}

	free(xSwizzle);
	return true;
}

// UNTESTED - Need test-case! (Sorry I wasn't able to find a game using this format)
bool WndMain::ReadSwizzled16bitFormatIntoBitmap(uint32 format, unsigned char *data, uint32 dataSize, int width, int height, int pitch, void*& bitmap)
{
	uint32* xSwizzle;
	int x, y;
	uint32	ySwizzle;
	TRGB16* yscanline;

	// sanity checks
	if (format != X_D3DFMT_R5G6B5)
		return false;
	if (!(width > 0) || !(height > 0))
		return false;

	xSwizzle = (uint32*)malloc(sizeof(uint32) * width);
	if (xSwizzle == NULL)
		return false;

	for (x = 0; x < width; x++)
		xSwizzle[x] = Swizzle(x, (height * 2), 0);

	// Loop over all lines :
	for (y = 0; y < height; y++)
	{
		// Calculate y-swizzle :
		ySwizzle = Swizzle(y, width, 1);

		// Copy whole line in one go (using pre-calculated x-swizzle) :
		yscanline = &((TRGB16*)bitmap)[y*(pitch / sizeof(TRGB16))];
		for (x = 0; x < width; x++)
			yscanline[x] = ((TRGB16*)data)[xSwizzle[x] + ySwizzle];
	}

	free(xSwizzle);
	return true;
}
#endif