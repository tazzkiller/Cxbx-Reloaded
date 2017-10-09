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
// *   Cxbx->Win32->CxbxKrnl->ResourceTracker.cpp
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
#include "ResourceTracker.h"

#include <assert.h>

// exported globals

bool g_bVBSkipStream = false;
bool g_bVBSkipPusher = false;

//
// all of our resource trackers
//

ResourceTracker g_VBTrackTotal;
ResourceTracker g_VBTrackDisable;
ResourceTracker g_PBTrackTotal;
ResourceTracker g_PBTrackDisable;
ResourceTracker g_PBTrackShowOnce;
#if 0 // unuseed
ResourceTracker g_DataToTexture;
ResourceTracker g_AlignCache;
#endif

ResourceTracker::~ResourceTracker()
{
    clear();
}

bool ResourceTracker::dispose(RTNode *cur)
{
	if (cur) {
		void *pResource = cur->pResource;
		delete cur;
		if (pResource) { // Note : nullptr isn't added, but m_tail has no pResource and gets here too
			if (m_callback) {
				// call on-delete-callback to avoid resource leaks
				m_callback(pResource);
				return true;
			}
		}
	}

	return false;
}

void ResourceTracker::clear()
{
    this->Lock();
    RTNode *cur = m_head;
    m_head = m_tail = nullptr;
    this->Unlock();

    while(cur != nullptr) { // Note : cannot check against m_tail, it's cleared already
        RTNode *tmp = cur->pNext;
		dispose(cur);
        cur = tmp;
    }
}

void ResourceTracker::insert(void *pResource)
{
    insert(pResource, pResource);
}

void ResourceTracker::insert(void *pKey, void *pResource)
{
	// Only insert when resource is assigned
	if (pResource == nullptr)
		return;

    this->Lock();
	if (get(pKey) == nullptr) {
		if (m_head == nullptr) {
			m_head = m_tail = new RTNode();
		}

		m_tail->pKey = pKey;
		m_tail->pResource = pResource;
		m_tail->pNext = new RTNode();
		m_tail = m_tail->pNext;
		m_tail->pKey = m_tail->pResource = m_tail->pNext = nullptr;
	}

	this->Unlock();
}

void *ResourceTracker::remove(void *pKey)
{
    RTNode *pre = nullptr;
    this->Lock();
    RTNode *cur = m_head;
    while(cur != m_tail) {
		if (cur->pKey == pKey) {
			if (pre != nullptr)
				pre->pNext = cur->pNext;
			else {
				m_head = cur->pNext;
				if (m_head == m_tail) {
					assert(m_head->pKey == nullptr);
					assert(m_head->pNext == nullptr);
					assert(m_head->pResource == nullptr);

					delete m_head;
					m_head = m_tail = nullptr;
				}
			}

			break;
		}

        pre = cur;
        cur = cur->pNext;
    }

    this->Unlock();
	if (cur) {
		void *res = cur->pResource;
		if (!dispose(cur))
			// When dispose didn't do a callback, returning res
			return res;
	}

	return nullptr;
}

void *ResourceTracker::exists(void *pKey)
{
	void *res = nullptr;
	this->Lock();
    RTNode *cur = m_head;
    while(cur != m_tail) {
        if(cur->pKey == pKey) {
            res = cur->pResource;
			break;
        }

        cur = cur->pNext;
    }

    this->Unlock();
    return res;
}

struct RTNode *ResourceTracker::getHead()
{
	assert(IsLocked());

	return m_head;
}

void *ResourceTracker::get(void *pKey)
{
	assert(IsLocked());

    RTNode *cur = m_head;
    while(cur != m_tail) {
        if(cur->pKey == pKey) {
            return cur->pResource;
        }

        cur = cur->pNext;
    }

    return nullptr;
}

uint32 ResourceTracker::get_count(void)
{
    uint32 uiCount = 0;
    this->Lock();
    RTNode *cur = m_head;
    while(cur != m_tail) {
        uiCount++;
        cur = cur->pNext;
    }

    this->Unlock();

    return uiCount;
}
