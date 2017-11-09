#pragma once

#ifndef xbaddr
typedef unsigned long xbaddr;
#endif

#define KB(x) ((x) *    1024 ) // = 0x00000400
#define MB(x) ((x) * KB(1024)) // = 0x00100000

typedef enum {
	MemLowVirtual,
	MemPhysical,
	MemPageTable,
	DeviceNV2A,
	MemNV2APRAMIN,
	DeviceAPU,
	DeviceAC97,
	DeviceUSB0,
	DeviceUSB1,
	DeviceNVNet,
	DeviceFlash,
	DeviceMCPX,
} XboxAddressRangeType;

typedef struct { 
	XboxAddressRangeType Type;
	xbaddr Start; 
	int Size;
} XboxAddressRange;

XboxAddressRange XboxAddressRanges[] = {
	// See http://xboxdevwiki.net/Memory
	// and http://xboxdevwiki.net/Boot_Process#Paging
	{ MemLowVirtual, 0x00000000, MB(128) }, // .. 0x08000000 (Retail Xbox uses 64 MB)
	{ MemPhysical,   0x80000000, MB(128) }, // .. 0x88000000 (Retail Xbox uses 64 MB)
	{ MemPageTable,  0xC0000000, KB(128) }, // .. 0xC0020000
	{ DeviceNV2A,    0xFD000000, MB(  7) }, // .. 0xFD6FFFFF (GPU)
	{ MemNV2APRAMIN, 0xFD700000, MB(  1) }, // .. 0xFD800000
	{ DeviceNV2A,    0xFD800000, MB(  8) }, // .. 0xFE000000 (GPU)
	{ DeviceAPU,     0xFE800000, KB(512) }, // .. 0xFE880000
	{ DeviceAC97,    0xFEC00000, KB(  4) }, // .. 0xFEC01000 (ACI)
	{ DeviceUSB0,    0xFED00000, KB(  4) }, // .. 0xFED01000
	{ DeviceUSB1,    0xFED08000, KB(  4) }, // .. 0xFED09000
	{ DeviceNVNet,   0xFEF00000, KB(  1) }, // .. 0xFEF00400
	{ DeviceFlash,   0xFF000000, MB(  4) }, // .. 0xFF3FFFFF (Flash mirror 1)
	{ DeviceFlash,   0xFF400000, MB(  4) }, // .. 0xFF7FFFFF (Flash mirror 2)
	{ DeviceFlash,   0xFF800000, MB(  4) }, // .. 0xFFBFFFFF (Flash mirror 3)
	{ DeviceFlash,   0xFFC00000, MB(  4) }, // .. 0xFFFFFFFF (Flash mirror 4) - Will probably fail reservation
	{ DeviceMCPX,    0xFFFFFE00,    512  }, // .. 0xFFFFFFFF (not Chihiro, Xbox - if enabled)
};