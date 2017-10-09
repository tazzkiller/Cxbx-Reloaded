// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->EmuD3D8->VertexBuffer.h
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
#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H

#include "Cxbx.h"
//#include <ctime> // Conflict with io.h

#define MAX_NBR_STREAMS 16

typedef struct _CxbxDrawContext
{
    IN     X_D3DPRIMITIVETYPE    XboxPrimitiveType;
    IN     DWORD                 dwVertexCount;
    IN     DWORD                 dwStartVertex; // Only D3DDevice_DrawVertices sets this (potentially higher than default 0)
    // The current vertex shader, used to identify the streams
    IN     DWORD                 hVertexShader;
    // Data if Draw...UP call
    IN PVOID                     pXboxVertexStreamZeroData;
    IN UINT                      uiXboxVertexStreamZeroStride;
	// Values to be used on host
	OUT PVOID                    pHostVertexStreamZeroData;
	OUT UINT                     uiHostVertexStreamZeroStride;
    OUT DWORD                    dwHostPrimitiveCount;
} CxbxDrawContext;

typedef struct _CxbxPatchedStream
{
	void                   *pCachedXboxVertexData;
    UINT                    uiCachedXboxVertexStride;
    uint32                  uiCachedXboxVertexDataSize;
    UINT                    uiCachedHostVertexStride;
    bool                    bCacheIsStreamZeroDrawUP;
    void                   *pCachedHostVertexStreamZeroData;
	bool                    bCachedHostVertexStreamZeroDataIsAllocated;
    IDirect3DVertexBuffer8 *pCachedHostVertexBuffer;
    bool                    bCacheIsUsed;
} CxbxPatchedStream;

typedef struct _CxbxCachedStream
{
    uint32_t       uiHash;
    uint32         uiCheckFrequency;
    uint32         uiCheckCount;        // XXHash32::hash() check count
    uint32         uiCacheHitCount;
    long           lLastUsed;           // For cache removal purposes
    CxbxPatchedStream  Stream;
} CxbxCachedStream;

class CxbxVertexBufferConverter
{
    public:
        CxbxVertexBufferConverter();
       ~CxbxVertexBufferConverter();

        void Apply(CxbxDrawContext *pDrawContext);
        void Restore();

        // Dumps the cache to the console
        static void DumpCache(void);

    private:

        UINT m_uiNbrStreams;
        CxbxPatchedStream m_PatchedStreams[MAX_NBR_STREAMS];

        CxbxVertexShaderDynamicPatch *m_pVertexShaderDynamicPatch;

        // Caches a patched stream
        void CacheStream(CxbxPatchedStream *pPatchedStream);

        // Frees a cached, patched stream
        void FreeCachedStream(void *pKey);

        // Tries to apply a previously patched stream from the cache
        bool ApplyCachedStream(CxbxDrawContext *pDrawContext,
                               UINT             uiStream,
							   bool			   *pbFatalError);

        // Convert the contents of the stream
        bool ConvertStream(CxbxDrawContext *pDrawContext, UINT uiStream);
};

// inline vertex buffer emulation
extern X_D3DPRIMITIVETYPE      g_InlineVertexBuffer_PrimitiveType;
extern DWORD                   g_InlineVertexBuffer_FVF;

#define MAX_PUSHBUFFER_DWORDS 2047 // Max nr of DWORD for D3DPUSH_ENCODE
#define INLINE_VERTEX_BUFFER_SIZE 2 * MAX_PUSHBUFFER_DWORDS // stay on the safe side

extern struct _D3DIVB
{
    XTL::D3DXVECTOR3 Position;
    FLOAT            Rhw;
	FLOAT			 Blend1;
	FLOAT			 Blend2;	 // Dxbx addition : for D3DFVF_XYZB2 TODO : Where should we set these?
	FLOAT			 Blend3;	 // Dxbx addition : for D3DFVF_XYZB3
	FLOAT			 Blend4;	 // Dxbx addition : for D3DFVF_XYZB4
    XTL::D3DXVECTOR3 Normal;
	// FLOAT            PointSize; // TODO : Is this in here?
	D3DCOLOR		Diffuse;
	D3DCOLOR		Specular;
#if 0
	FLOAT            Fog; // TODO : Handle
	D3DCOLOR		BackDiffuse; // TODO : Handle
	D3DCOLOR		BackSpecular; // TODO  : Handle
#endif
    XTL::D3DXVECTOR3 TexCoord1;
    XTL::D3DXVECTOR3 TexCoord2;
    XTL::D3DXVECTOR3 TexCoord3;
    XTL::D3DXVECTOR3 TexCoord4;
}
*g_InlineVertexBuffer_Table;

extern UINT g_InlineVertexBuffer_TableLength;
extern UINT g_InlineVertexBuffer_TableOffset;

extern VOID EmuFlushIVB();

#endif