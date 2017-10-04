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
ResourceTracker g_PatchedStreamsCache;
#if 0 // unuseed
ResourceTracker g_DataToTexture;
#endif
//ResourceTracker g_AlignCache;

ResourceTracker::~ResourceTracker()
{
    clear();
}

void ResourceTracker::clear()
{
    this->Lock();
    RTNode *cur = m_head;
    while(cur != nullptr) {
        RTNode *tmp = cur->pNext;
        delete cur;
        cur = tmp;
    }

    m_head = m_tail = nullptr;
    this->Unlock();
}

void ResourceTracker::insert(void *pResource)
{
    insert(pResource, pResource);
}

void ResourceTracker::insert(void *pKey, void *pResource)
{
    this->Lock();
	if (!exists(pKey)) {
		if (m_head == nullptr) {
			m_head = m_tail = new RTNode();
		}

		m_tail->pResource = pResource;
		m_tail->pKey = pKey;
		m_tail->pNext = new RTNode();
		m_tail = m_tail->pNext;
		m_tail->pKey = m_tail->pResource = NULL;
		m_tail->pNext = nullptr;
	}

	this->Unlock();
}

void ResourceTracker::remove(void *pKey)
{
    RTNode *pre = nullptr;
    this->Lock();
    RTNode *cur = m_head;
    while(cur != nullptr) {
        if(cur->pKey == pKey) {
            if(pre != nullptr) {
                pre->pNext = cur->pNext;
            } else {
                m_head = cur->pNext;
                if(m_head->pNext == nullptr) {
                    delete m_head;
                    m_head = m_tail = nullptr;
                }
            }

            delete cur;
			break;
        }

        pre = cur;
        cur = cur->pNext;
    }

    this->Unlock();
}

bool ResourceTracker::exists(void *pKey)
{
    this->Lock();
    RTNode *cur = m_head;
    while(cur != nullptr) {
        if(cur->pKey == pKey) {
            this->Unlock();

            return true;
        }

        cur = cur->pNext;
    }

    this->Unlock();

    return false;
}

void *ResourceTracker::get(void *pKey)
{
    RTNode *cur = m_head;
    while(cur != nullptr) {
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
    while(cur != nullptr) {
        uiCount++;
        cur = cur->pNext;
    }

    this->Unlock();

    return uiCount;
}
