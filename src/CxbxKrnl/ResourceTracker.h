// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->ResourceTracker.h
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
#ifndef RESOURCETRACKER_H
#define RESOURCETRACKER_H

#include "Cxbx.h"
#include "Common/Win32/Mutex.h"

extern bool g_bVBSkipStream;
extern bool g_bPBSkipPusher;

typedef void (*ResourceCleanup)(void *pResource);

extern class ResourceTracker : public Mutex
{
    public:
        ResourceTracker(ResourceCleanup callback = nullptr) : m_callback(callback), m_head(nullptr), m_tail(nullptr) {};
       ~ResourceTracker();

        // clear the tracker
        void clear();

        // insert a ptr using the pResource pointer as key and resource
        void insert(void *pResource);

        // insert a ptr using an explicit key (unless resouce is a nullptr)
        void insert(void *pKey, void *pResource);

        // remove a ptr using an explicit key
        void *remove(void *pKey);

        // check for existance of an explicit key, return associated resource if it exists, nullptr otherwise
        void *exists(void *pKey);

        // retrieves a resource using an explicit key, explicit locking needed
        void *get(void *pKey);

        // retrieves the number of entries in the tracker
        uint32 get_count(void);

        // for traversal
		struct RTNode *getHead();

    private:
        // list of "live" vertex buffers for debugging purposes
        struct RTNode *m_head;
        struct RTNode *m_tail;
		ResourceCleanup m_callback;

		// deletes a RTNode, calling cleanup on it's resource
	    bool dispose(RTNode *cur);
}
g_VBTrackTotal, 
g_VBTrackDisable,
g_PBTrackTotal, 
g_PBTrackDisable, 
g_PBTrackShowOnce,
g_PatchedStreamsCache 
#if 0 // unused
, g_AlignCache
#endif
;

struct RTNode
{
    void    *pKey;
    void    *pResource;
    RTNode  *pNext;
};

#endif
