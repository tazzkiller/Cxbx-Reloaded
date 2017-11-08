#pragma once

#ifndef xbaddr
typedef unsigned long xbaddr;
#endif

#define MB(x) ((x) * 1024*1024)

typedef enum {
	ImageBase,
	Physical,
	PageTable,
	DeviceNV2A,
} XboxMemoryRangeType;

typedef struct { 
	xbaddr Start; 
	int Size;
	XboxMemoryRangeType Type;
} XboxMemoryRange;

XboxMemoryRange XboxMemoryRanges[] = {
	{          0, MB(128),      ImageBase  }, // Note 0 is a marker, indicating the first page above the loader executable itself (0x00010000 + image-size)
	{ 0x80000000, MB(128),      Physical   },
	{ 0xC0000000, MB(1),        PageTable  },
	{ 0xFD000000, 0x01000000,   DeviceNV2A }
};