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
    IN OUT X_D3DPRIMITIVETYPE    XboxPrimitiveType;
    IN     DWORD                 dwVertexCount;
    OUT    DWORD                 dwPrimitiveCount;
    IN     DWORD                 dwStartVertex; // Only D3DDevice_DrawVertices sets this (potentially higher than default 0)
    // Data if Draw...UP call
    IN OUT PVOID                 pVertexStreamZeroData;
    IN OUT UINT                  uiVertexStreamZeroStride;
    // The current vertex shader, used to identify the streams
    IN     DWORD                 hVertexShader;
} CxbxDrawContext;

typedef struct _CxbxPatchedStream
{
	void                   *pXboxVertexData;
    UINT                    uiOrigStride;
    IDirect3DVertexBuffer8 *pHostVertexBuffer;
    UINT                    uiNewStride;
    bool                    bUsedCached;
} CxbxPatchedStream;

typedef struct _CxbxCachedStream
{
    uint32_t       uiHash;
    uint32         uiCheckFrequency;
    uint32         uiCheckCount;        // XXHash32::hash() check count
    uint32         uiCacheHit;
    long           lLastUsed;           // For cache removal purposes
    bool           bIsUP;
    CxbxPatchedStream  Stream;
    void          *pStreamUP;           // Draw..UP (instead of pOriginalStream)
    uint32         uiLength;            // The length of the stream
	CxbxDrawContext Copy;
} CxbxCachedStream;

class CxbxVertexBufferConverter
{
    public:
        CxbxVertexBufferConverter();
       ~CxbxVertexBufferConverter();

        bool Apply(CxbxDrawContext *pDrawContext);
        void Restore();

        // Dumps the cache to the console
        static void DumpCache(void);

    private:

        UINT m_uiNbrStreams;
        CxbxPatchedStream m_pStreams[MAX_NBR_STREAMS];

        PVOID m_pNewVertexStreamZeroData;

        bool m_bPatched;
        bool m_bAllocatedStreamZeroData;

        CxbxVertexDynamicPatch *m_pDynamicPatch;

        // Caches a patched stream
        void CacheStream(CxbxDrawContext *pDrawContext,
                         UINT             uiStream);

        // Frees a cached, patched stream
        void FreeCachedStream(void *pStream);

        // Tries to apply a previously patched stream from the cache
        bool ApplyCachedStream(CxbxDrawContext *pDrawContext,
                               UINT             uiStream,
							   bool			   *pbFatalError);

        // Convert the contents of the stream
        void ConvertStream(CxbxDrawContext *pDrawContext, UINT uiStream);
};

// inline vertex buffer emulation
extern PVOID                   g_InlineVertexBuffer_pData;
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
	XTL::D3DCOLOR    Diffuse;
	XTL::D3DCOLOR    Specular;
#if 0
	FLOAT            Fog; // TODO : Handle
	XTL::D3DCOLOR    BackDiffuse; // TODO : Handle
	XTL::D3DCOLOR    BackSpecular; // TODO  : Handle
#endif
    XTL::D3DXVECTOR3 TexCoord1;
    XTL::D3DXVECTOR3 TexCoord2;
    XTL::D3DXVECTOR3 TexCoord3;
    XTL::D3DXVECTOR3 TexCoord4;
}
*g_InlineVertexBuffer_Table;

extern UINT g_InlineVertexBuffer_TableOffset;

extern VOID EmuFlushIVB();

#endif