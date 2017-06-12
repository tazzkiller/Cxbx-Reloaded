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

typedef struct _VertexPatchDesc
{
    IN OUT X_D3DPRIMITIVETYPE    XboxPrimitiveType;
    IN     DWORD                 dwVertexCount;
    OUT    DWORD                 dwPrimitiveCount;
    IN     DWORD                 dwOffset;
    // Data if Draw...UP call
    IN OUT PVOID                 pVertexStreamZeroData;
    IN OUT UINT                  uiVertexStreamZeroStride;
    // The current vertex shader, used to identify the streams
    IN     DWORD                 hVertexShader;
}
VertexPatchDesc;

typedef struct _PATCHEDSTREAM
{
	void                   *pXboxVertexData;
    UINT                    uiOrigStride;
    IDirect3DVertexBuffer8 *pHostVertexBuffer;
    UINT                    uiNewStride;
    bool                    bUsedCached;
} PATCHEDSTREAM;

typedef struct _CACHEDSTREAM
{
    uint32_t       uiHash;
    uint32         uiCheckFrequency;
    uint32         uiCheckCount;        // XXHash32::hash() check count
    uint32         uiCacheHit;
    long           lLastUsed;           // For cache removal purposes
    bool           bIsUP;
    PATCHEDSTREAM  Stream;
    void          *pStreamUP;           // Draw..UP (instead of pOriginalStream)
    uint32         uiLength;            // The length of the stream
	VertexPatchDesc Copy;
} CACHEDSTREAM;

class VertexPatcher
{
    public:
        VertexPatcher();
       ~VertexPatcher();

        bool Apply(VertexPatchDesc *pPatchDesc);
        void Restore();

        // Dumps the cache to the console
        static void DumpCache(void);

    private:

        UINT m_uiNbrStreams;
        PATCHEDSTREAM m_pStreams[MAX_NBR_STREAMS];

        PVOID m_pNewVertexStreamZeroData;

        bool m_bPatched;
        bool m_bAllocatedStreamZeroData;

        VERTEX_DYNAMIC_PATCH *m_pDynamicPatch;

        // Returns the number of streams of a patch
        UINT GetNbrStreams(VertexPatchDesc *pPatchDesc);

        // Caches a patched stream
        void CacheStream(VertexPatchDesc *pPatchDesc,
                         UINT             uiStream);

        // Frees a cached, patched stream
        void FreeCachedStream(void *pStream);

        // Tries to apply a previously patched stream from the cache
        bool ApplyCachedStream(VertexPatchDesc *pPatchDesc,
                               UINT             uiStream,
							   bool			   *pbFatalError);

        // Convert the contents of the stream
        void ConvertStream(VertexPatchDesc *pPatchDesc, UINT uiStream);
};

// inline vertex buffer emulation
extern PVOID                   g_InlineVertexBuffer_pData;
extern X_D3DPRIMITIVETYPE      g_InlineVertexBuffer_PrimitiveType;
extern DWORD                   g_InlineVertexBuffer_FVF;

#define INLINE_VERTEX_BUFFER_SIZE 2047 // Max nr of DWORD for D3DPUSH_ENCODE

extern struct _D3DIVB
{
    XTL::D3DXVECTOR3 Position;
    FLOAT            Rhw;
	FLOAT			 Blend1;
	FLOAT			 Blend2;	 // Dxbx addition : for D3DFVF_XYZB2 TODO : Where should we set these?
	FLOAT			 Blend3;	 // Dxbx addition : for D3DFVF_XYZB3
	FLOAT			 Blend4;	 // Dxbx addition : for D3DFVF_XYZB4
	XTL::D3DCOLOR    Specular;
    XTL::D3DCOLOR    Diffuse;
    XTL::D3DXVECTOR3 Normal;
    XTL::D3DXVECTOR2 TexCoord1;
    XTL::D3DXVECTOR2 TexCoord2;
    XTL::D3DXVECTOR2 TexCoord3;
    XTL::D3DXVECTOR2 TexCoord4;
}
*g_InlineVertexBuffer_Table;

extern UINT g_InlineVertexBuffer_TableOffset;

extern VOID EmuFlushIVB();

#endif