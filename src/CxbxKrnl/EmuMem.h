// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->EmuMem.h
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
// *  (c) 2016 Patrick van Logchem <pvanlogchem@gmail.com>
// *  All rights reserved
// *
// ******************************************************************
#ifndef EMUMEM_H
#define EMUMEM_H

#pragma once

#include <cstdint> // For uint8_t, etc.

#define FASTCALL __fastcall

#define HANDLER_API	FASTCALL

// From MAME osdcomm.h :

/* Some optimizations/warnings cleanups for GCC */
#if defined(__GNUC__)
#define ATTR_UNUSED             __attribute__((__unused__))
#define ATTR_PRINTF(x,y)        __attribute__((format(printf, x, y)))
#define ATTR_CONST              __attribute__((const))
#define ATTR_FORCE_INLINE       __attribute__((always_inline))
#define ATTR_HOT                __attribute__((hot))
#define ATTR_COLD               __attribute__((cold))
#define UNEXPECTED(exp)         __builtin_expect(!!(exp), 0)
#define EXPECTED(exp)           __builtin_expect(!!(exp), 1)
#define RESTRICT                __restrict__
#else
#define ATTR_UNUSED
#define ATTR_PRINTF(x,y)
#define ATTR_CONST
#define ATTR_FORCE_INLINE       __forceinline
#define ATTR_HOT
#define ATTR_COLD
#define UNEXPECTED(exp)         (exp)
#define EXPECTED(exp)           (exp)
#define RESTRICT
#endif

// explicitly sized integers
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// From MAME emumem.h :

enum { TOTAL_MEMORY_BANKS = 512 };

// offsets and addresses are 32-bit (for now...)
typedef u32 offs_t;

static const int LEVEL1_BITS = 14;                       // number of address bits in the level 1 table
// Note : Cxbx uses 14 (since we store two handler pointers), but MAME uses 18 (storing u16 codes)
static const int LEVEL2_BITS = 32 - LEVEL1_BITS;         // number of address bits in the level 2 table

// Cxbx specific

// space read/write handler function macros
#define READ8_MEMBER(name)   u8   HANDLER_API name(ATTR_UNUSED offs_t offset)
#define WRITE8_MEMBER(name)  void HANDLER_API name(ATTR_UNUSED offs_t offset, ATTR_UNUSED u8 data)
#define READ16_MEMBER(name)  u16  HANDLER_API name(ATTR_UNUSED offs_t offset)
#define WRITE16_MEMBER(name) void HANDLER_API name(ATTR_UNUSED offs_t offset, ATTR_UNUSED u16 data)
#define READ32_MEMBER(name)  u32  HANDLER_API name(ATTR_UNUSED offs_t offset)
#define WRITE32_MEMBER(name) void HANDLER_API name(ATTR_UNUSED offs_t offset, ATTR_UNUSED u32 data)
#define READ64_MEMBER(name)  u64  HANDLER_API name(ATTR_UNUSED offs_t offset)
#define WRITE64_MEMBER(name) void HANDLER_API name(ATTR_UNUSED offs_t offset, ATTR_UNUSED u64 data)

// handler function pointer types
typedef	u8   (HANDLER_API *handle8_r )(ATTR_UNUSED offs_t offset);
typedef void (HANDLER_API *handle8_w )(ATTR_UNUSED offs_t offset, ATTR_UNUSED u8 data);
typedef	u16  (HANDLER_API *handle16_r)(ATTR_UNUSED offs_t offset);
typedef void (HANDLER_API *handle16_w)(ATTR_UNUSED offs_t offset, ATTR_UNUSED u16 data);
typedef	u32  (HANDLER_API *handle32_r)(ATTR_UNUSED offs_t offset);
typedef void (HANDLER_API *handle32_w)(ATTR_UNUSED offs_t offset, ATTR_UNUSED u32 data);
typedef	u64  (HANDLER_API *handle64_r)(ATTR_UNUSED offs_t offset);
typedef void (HANDLER_API *handle64_w)(ATTR_UNUSED offs_t offset, ATTR_UNUSED u64 data);

const int LEVEL1_SIZE = 1 << LEVEL1_BITS;

// array of read/write handlers
extern handle32_r g_AddrLvl1_r[LEVEL1_SIZE];
extern handle32_w g_AddrLvl1_w[LEVEL1_SIZE];
// Note : Cxbx stores handlers directly, to avoid an indirection (like MAME does)
// Note : Cxbx stores 32 bit handlers only, assuming we don't need the other bitnesses
// Note : Cxbx stores read handlers separate from write handlers, to reduce cache pollution.

// read/write host memory
READ32_MEMBER(mem_host32_r);
WRITE32_MEMBER(mem_host32_w);

void mem_handlers_init();

void mem_handler_install_r(offs_t offset, offs_t mask, handle32_r handler_r);
void mem_handler_install_w(offs_t offset, offs_t mask, handle32_w handler_w);

#endif

