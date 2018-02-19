// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->src->CxbxKrnl->EmuKrnlKi.h
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
// *  (c) 2018 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#pragma once

#define KiLockDispatcherDatabase(OldIrql)      \
	*(OldIrql) = KeRaiseIrqlToDpcLevel()

#define KiLockApcQueue(Thread, OldIrql)        \
    *(OldIrql) = KeRaiseIrqlToSynchLevel()

#define KiUnlockApcQueue(Thread, OldIrql)      \
	KfLowerIrql((OldIrql))

#define KiRemoveTreeTimer(Timer)               \
    (Timer)->Header.Inserted = FALSE;          \
    RemoveEntryList(&(Timer)->TimerListEntry)

void KiDeliverApc();

void KiDeliverUserApc();

xboxkrnl::BOOLEAN KiInsertTreeTimer(
	IN xboxkrnl::PKTIMER Timer,
	IN xboxkrnl::LARGE_INTEGER Interval
);

#define KiWaitSatisfyMutant(_Object_, _Thread_) { \
    (_Object_)->Header.SignalState -= 1; \
    if ((_Object_)->Header.SignalState == 0) { \
        (_Object_)->OwnerThread = _Thread_; \
        if ((_Object_)->Abandoned) { \
            (_Object_)->Abandoned = FALSE; \
            (_Thread_)->WaitStatus = STATUS_ABANDONED; \
        } \
        InsertHeadList(_Thread_->MutantListHead.Blink, \
                       &(_Object_)->MutantListEntry); \
    } \
}

#define KiWaitSatisfyOther(_Object_) { \
    if (((_Object_)->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == KOBJECTS::EventSynchronizationObject) { \
        (_Object_)->Header.SignalState = 0;\
    } else if ((_Object_)->Header.Type == KOBJECTS::SemaphoreObject) { \
        (_Object_)->Header.SignalState -= 1; \
    } \
}

void KiWaitSatisfyAny(xboxkrnl::PKMUTANT Object, xboxkrnl::PKTHREAD Thread);

void KiWaitSatisfyAll(xboxkrnl::PKWAIT_BLOCK WaitBlock);