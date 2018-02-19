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
// *   Cxbx->src->CxbxKrnl->EmuKrnlKi.cpp
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
// *  (c) 2016 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _XBOXKRNL_DEFEXTRN_

#define LOG_PREFIX "KRNL"

// prevent name collisions
namespace xboxkrnl
{
#include <xboxkrnl/xboxkrnl.h> // For KeBugCheck, etc.
};

#include "Logging.h" // For LOG_FUNC()
#include "EmuKrnlLogging.h"

#include "EmuKrnl.h" // For InitializeListHead(), etc.
#include "EmuKrnlKi.h" // For InitializeListHead(), etc.

void KiDeliverApc() {
	xboxkrnl::PKTHREAD pThread = xboxkrnl::KeGetCurrentThread();

	xboxkrnl::KIRQL oldIRQL;
	{ using namespace xboxkrnl;
	KiLockApcQueue(thread, &oldIRQL);
	}

	pThread->ApcState.KernelApcPending = FALSE;
	while (!IsListEmpty(&pThread->ApcState.ApcListHead[xboxkrnl::KernelMode])) {
		auto pNextEntry = NextListEntry(&pThread->ApcState.ApcListHead[xboxkrnl::KernelMode]);
		xboxkrnl::KAPC *pApc = CONTAINING_RECORD(pNextEntry, xboxkrnl::KAPC, ApcListEntry);

		{ using namespace xboxkrnl;
		PKKERNEL_ROUTINE KernelRoutine = pApc->KernelRoutine;
		PKNORMAL_ROUTINE NormalRoutine = pApc->NormalRoutine;
		PVOID NormalContext = pApc->NormalContext;
		PVOID SystemArgument1 = pApc->SystemArgument1;
		PVOID SystemArgument2 = pApc->SystemArgument2;

		if (NormalRoutine == NULL) {
			RemoveEntryList(pNextEntry);
			pApc->Inserted = FALSE;

			KiUnlockApcQueue(thread, oldIRQL);
			KernelRoutine(pApc, &NormalRoutine, &NormalContext, &SystemArgument1, &SystemArgument2);
			KiLockApcQueue(thread, &oldIRQL);
		}
		else {
			if (!pThread->ApcState.KernelApcInProgress && pThread->KernelApcDisable == 0) {
				RemoveEntryList(pNextEntry);
				pApc->Inserted = FALSE;
				KiUnlockApcQueue(thread, oldIRQL);
				KernelRoutine(pApc, &NormalRoutine, &NormalContext, &SystemArgument1, &SystemArgument2);

				if (NormalRoutine != NULL) {
					pThread->ApcState.KernelApcInProgress = TRUE;
					KfLowerIrql(0);
					NormalRoutine(NormalContext, SystemArgument1, SystemArgument2);
					oldIRQL = KfRaiseIrql(APC_LEVEL);
				}

				KiLockApcQueue(thread, &oldIRQL);
				pThread->ApcState.KernelApcInProgress = FALSE;
			}
			else {
				KiUnlockApcQueue(thread, oldIRQL);
				return;
			}
		}
		}
	}

	{ using namespace xboxkrnl;
	KiUnlockApcQueue(thread, oldIRQL);
	}
}

void KiDeliverUserApc()
{
	xboxkrnl::PKTHREAD pThread = xboxkrnl::KeGetCurrentThread();
	xboxkrnl::KIRQL oldIRQL;

	{ using namespace xboxkrnl;
	KiLockApcQueue(thread, &oldIRQL);
	}

	pThread->ApcState.UserApcPending = FALSE;
	while (!IsListEmpty(&pThread->ApcState.ApcListHead[xboxkrnl::UserMode])) {
		auto pNextEntry = NextListEntry(&pThread->ApcState.ApcListHead[xboxkrnl::UserMode]);
		xboxkrnl::KAPC *pApc = CONTAINING_RECORD(pNextEntry, xboxkrnl::KAPC, ApcListEntry);

		{ using namespace xboxkrnl;
		PKKERNEL_ROUTINE KernelRoutine = pApc->KernelRoutine;
		PKNORMAL_ROUTINE NormalRoutine = pApc->NormalRoutine;
		PVOID NormalContext = pApc->NormalContext;
		PVOID SystemArgument1 = pApc->SystemArgument1;
		PVOID SystemArgument2 = pApc->SystemArgument2;

		RemoveEntryList(pNextEntry);
		pApc->Inserted = FALSE;

		KiUnlockApcQueue(thread, oldIRQL);
		KernelRoutine(pApc, &NormalRoutine, &NormalContext, &SystemArgument1, &SystemArgument2);

		if ((void*)NormalRoutine != NULL) {
			NormalRoutine(NormalContext, SystemArgument1, SystemArgument2);
		}

		KiLockApcQueue(thread, &oldIRQL);
		}
	}

	{ using namespace xboxkrnl;
	KiUnlockApcQueue(thread, oldIRQL);
	}
}

xboxkrnl::BOOLEAN KiInsertTimerTable(
	IN xboxkrnl::LARGE_INTEGER Interval,
	xboxkrnl::ULONGLONG,
	IN xboxkrnl::PKTIMER Timer
)
{
	// TODO
	return TRUE;
}

xboxkrnl::BOOLEAN KiInsertTreeTimer(
	IN xboxkrnl::PKTIMER Timer,
	IN xboxkrnl::LARGE_INTEGER Interval
)
{
	// Is the given time absolute (indicated by a positive number)?
	if (Interval.u.HighPart >= 0) {
		// Convert absolute time to a time relative to the system time :
		xboxkrnl::LARGE_INTEGER SystemTime;
		xboxkrnl::KeQuerySystemTime(&SystemTime);
		Interval.QuadPart = SystemTime.QuadPart - Interval.QuadPart;
		if (Interval.u.HighPart >= 0) {
			// If the relative time is already passed, return without inserting :
			Timer->Header.Inserted = FALSE;
			Timer->Header.SignalState = TRUE;
			return FALSE;
		}

		Timer->Header.Absolute = TRUE;
	}
	else
		// Negative intervals are relative, not absolute :
		Timer->Header.Absolute = FALSE;

	if (Timer->Period == 0)
		Timer->Header.SignalState = FALSE;

	Timer->Header.Inserted = TRUE;
	return KiInsertTimerTable(Interval, xboxkrnl::KeQueryInterruptTime(), Timer);
}

void KiWaitSatisfyAny(xboxkrnl::PKMUTANT Object, xboxkrnl::PKTHREAD Thread) {
	using namespace xboxkrnl;

	if ((Object->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == KOBJECTS::EventSynchronizationObject) {
		Object->Header.SignalState = 0;
	}
	else if (Object->Header.Type == xboxkrnl::SemaphoreObject) {
		Object->Header.SignalState -= 1;
	}
	else if (Object->Header.Type == xboxkrnl::MutantObject) {
		Object->Header.SignalState -= 1;
		if (Object->Header.SignalState == 0) {
			Object->OwnerThread = Thread;
			if (Object->Abandoned == TRUE) {
				Object->Abandoned = FALSE;
				Thread->WaitStatus = STATUS_ABANDONED;
			}
			InsertHeadList(PrevListEntry(&Thread->MutantListHead), &Object->MutantListEntry);
		}
	}
}

void KiWaitSatisfyAll(xboxkrnl::PKWAIT_BLOCK WaitBlock) {
	using namespace xboxkrnl;

	KWAIT_BLOCK *pCurrentWaitBlock = WaitBlock;
	PKTHREAD thread = WaitBlock->Thread;
	do {
		if (pCurrentWaitBlock->WaitKey != STATUS_TIMEOUT) {
			KiWaitSatisfyAny((PKMUTANT)pCurrentWaitBlock->Object, thread);
		}
		pCurrentWaitBlock = (KWAIT_BLOCK*)(pCurrentWaitBlock->NextWaitBlock);
	} while (pCurrentWaitBlock != WaitBlock);
}
