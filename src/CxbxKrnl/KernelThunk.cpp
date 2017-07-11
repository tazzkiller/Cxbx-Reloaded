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
// *   CxbxKrnl->KernelThunk.cpp
// *
// *  This file is part of the Cxbx-Reloaded project, a fork of Cxbx.
// *
// *  Cxbx-Reloaded is free software; you can redistribute it
// *  and/or modify it under the terms of the GNU General Public
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
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

// prevent name collisions
namespace xboxkrnl
{
    #include <xboxkrnl/xboxkrnl.h>
};

#include "Cxbx.h" // For CxbxKrnl_KernelThunkTable
#include "CxbxKrnl.h" // For UINT

//
// Enable "#define PANIC(numb) numb" if you wish to find out what
// kernel export the application is attempting to call. The app
// will crash at the thunk number (i.e. PsQueryStatistics:0x0100)
//
// For general use, you should probably just enable the other
// option "#define PANIC(numb) cxbx_panic"
//
//#define PANIC(numb) CxbxKrnlPanic
#define PANIC(numb) (xbaddr)numb

#define FUNC(f) (xbaddr)&xboxkrnl::f
#define VARIABLE(v) (xbaddr)&xboxkrnl::v

#define DEVKIT // developer kit only functions
#define PROFILING // private kernel profiling functions
// A.k.a. _XBOX_ENABLE_PROFILING

// kernel thunk table
// Note : Names that collide with other symbols, use the KRNL() macro.
xbaddr CxbxKrnl_KernelThunkTable[379] =
{
	PANIC(0x0000),                             // 0x0000 (0) NULL
	FUNC(AvGetSavedDataAddress),               // 0x0001 (1)
	FUNC(AvSendTVEncoderOption),               // 0x0002 (2)
	FUNC(AvSetDisplayMode),                    // 0x0003 (3)
	FUNC(AvSetSavedDataAddress),               // 0x0004 (4)
	FUNC(DbgBreakPoint),                       // 0x0005 (5)
	FUNC(DbgBreakPointWithStatus),             // 0x0006 (6)
	FUNC(DbgLoadImageSymbols),                 // 0x0007 (7) DEVKIT
	FUNC(DbgPrint),                            // 0x0008 (8)
	FUNC(HalReadSMCTrayState),                 // 0x0009 (9)
	FUNC(DbgPrompt),                           // 0x000A (10)
	FUNC(DbgUnLoadImageSymbols),               // 0x000B (11) DEVKIT
	FUNC(ExAcquireReadWriteLockExclusive),     // 0x000C (12)
	FUNC(ExAcquireReadWriteLockShared),        // 0x000D (13)
	FUNC(ExAllocatePool),                      // 0x000E (14)
	FUNC(ExAllocatePoolWithTag),               // 0x000F (15)
	VARIABLE(ExEventObjectType),               // 0x0010 (16)
	FUNC(ExFreePool),                          // 0x0011 (17)
	FUNC(ExInitializeReadWriteLock),           // 0x0012 (18)
	FUNC(ExInterlockedAddLargeInteger),        // 0x0013 (19)
	FUNC(ExInterlockedAddLargeStatistic),      // 0x0014 (20)
	FUNC(ExInterlockedCompareExchange64),      // 0x0015 (21)
	VARIABLE(ExMutantObjectType),              // 0x0016 (22)
	FUNC(ExQueryPoolBlockSize),                // 0x0017 (23)
	FUNC(ExQueryNonVolatileSetting),           // 0x0018 (24)
	FUNC(ExReadWriteRefurbInfo),               // 0x0019 (25)
	FUNC(ExRaiseException),                    // 0x001A (26)
	FUNC(ExRaiseStatus),                       // 0x001B (27)
	FUNC(ExReleaseReadWriteLock),              // 0x001C (28)
	FUNC(ExSaveNonVolatileSetting),            // 0x001D (29)
	VARIABLE(ExSemaphoreObjectType),           // 0x001E (30)
	VARIABLE(ExTimerObjectType),               // 0x001F (31)
	FUNC(ExfInterlockedInsertHeadList),        // 0x0020 (32)
	FUNC(ExfInterlockedInsertTailList),        // 0x0021 (33)
	FUNC(ExfInterlockedRemoveHeadList),        // 0x0022 (34)
	FUNC(FscGetCacheSize),                     // 0x0023 (35)
	FUNC(FscInvalidateIdleBlocks),             // 0x0024 (36)
	FUNC(FscSetCacheSize),                     // 0x0025 (37)
	FUNC(HalClearSoftwareInterrupt),           // 0x0026 (38)
	FUNC(HalDisableSystemInterrupt),           // 0x0027 (39)
	VARIABLE(HalDiskCachePartitionCount),      // 0x0028 (40) // A.k.a. "IdexDiskPartitionPrefixBuffer"
	VARIABLE(HalDiskModelNumber),              // 0x0029 (41)
	VARIABLE(HalDiskSerialNumber),             // 0x002A (42)
	FUNC(HalEnableSystemInterrupt),            // 0x002B (43)
	FUNC(HalGetInterruptVector),               // 0x002C (44)
	FUNC(HalReadSMBusValue),                   // 0x002D (45)
	FUNC(HalReadWritePCISpace),                // 0x002E (46)
	FUNC(HalRegisterShutdownNotification),     // 0x002F (47)
	FUNC(HalRequestSoftwareInterrupt),         // 0x0030 (48)
	FUNC(HalReturnToFirmware),                 // 0x0031 (49)
	FUNC(HalWriteSMBusValue),                  // 0x0032 (50)
	FUNC(KRNL(InterlockedCompareExchange)),    // 0x0033 (51)
	FUNC(KRNL(InterlockedDecrement)),          // 0x0034 (52)
	FUNC(KRNL(InterlockedIncrement)),          // 0x0035 (53)
	FUNC(KRNL(InterlockedExchange)),           // 0x0036 (54)
	FUNC(KRNL(InterlockedExchangeAdd)),        // 0x0037 (55)
	FUNC(KRNL(InterlockedFlushSList)),         // 0x0038 (56)
	FUNC(KRNL(InterlockedPopEntrySList)),      // 0x0039 (57)
	FUNC(KRNL(InterlockedPushEntrySList)),     // 0x003A (58)
	FUNC(IoAllocateIrp),                       // 0x003B (59)
	FUNC(IoBuildAsynchronousFsdRequest),       // 0x003C (60)
	FUNC(IoBuildDeviceIoControlRequest),       // 0x003D (61)
	FUNC(IoBuildSynchronousFsdRequest),        // 0x003E (62)
	FUNC(IoCheckShareAccess),                  // 0x003F (63)
	VARIABLE(IoCompletionObjectType),          // 0x0040 (64)
	FUNC(IoCreateDevice),                      // 0x0041 (65)
	FUNC(IoCreateFile),                        // 0x0042 (66)
	FUNC(IoCreateSymbolicLink),                // 0x0043 (67)
	FUNC(IoDeleteDevice),                      // 0x0044 (68)
	FUNC(IoDeleteSymbolicLink),                // 0x0045 (69)
	VARIABLE(IoDeviceObjectType),              // 0x0046 (70)
	VARIABLE(IoFileObjectType),                // 0x0047 (71)
	FUNC(IoFreeIrp),                           // 0x0048 (72)
	FUNC(IoInitializeIrp),                     // 0x0049 (73)
	FUNC(IoInvalidDeviceRequest),              // 0x004A (74)
	FUNC(IoQueryFileInformation),              // 0x004B (75)
	FUNC(IoQueryVolumeInformation),            // 0x004C (76)
	FUNC(IoQueueThreadIrp),                    // 0x004D (77)
	FUNC(IoRemoveShareAccess),                 // 0x004E (78)
	FUNC(IoSetIoCompletion),                   // 0x004F (79)
	FUNC(IoSetShareAccess),                    // 0x0050 (80)
	FUNC(IoStartNextPacket),                   // 0x0051 (81)
	FUNC(IoStartNextPacketByKey),              // 0x0052 (82)
	FUNC(IoStartPacket),                       // 0x0053 (83)
	FUNC(IoSynchronousDeviceIoControlRequest), // 0x0054 (84)
	FUNC(IoSynchronousFsdRequest),             // 0x0055 (85)
	FUNC(IofCallDriver),                       // 0x0056 (86)
	FUNC(IofCompleteRequest),                  // 0x0057 (87)
	VARIABLE(KdDebuggerEnabled),               // 0x0058 (88)
	VARIABLE(KdDebuggerNotPresent),            // 0x0059 (89)
	FUNC(IoDismountVolume),                    // 0x005A (90)
	FUNC(IoDismountVolumeByName),              // 0x005B (91)
	FUNC(KeAlertResumeThread),                 // 0x005C (92)
	FUNC(KeAlertThread),                       // 0x005D (93)
	FUNC(KeBoostPriorityThread),               // 0x005E (94)
	FUNC(KeBugCheck),                          // 0x005F (95)
	FUNC(KeBugCheckEx),                        // 0x0060 (96)
	FUNC(KeCancelTimer),                       // 0x0061 (97)
	FUNC(KeConnectInterrupt),                  // 0x0062 (98)
	FUNC(KeDelayExecutionThread),              // 0x0063 (99)
	FUNC(KeDisconnectInterrupt),               // 0x0064 (100
	FUNC(KeEnterCriticalRegion),               // 0x0065 (101)
	VARIABLE(MmGlobalData),                    // 0x0066 (102)
	FUNC(KeGetCurrentIrql),                    // 0x0067 (103)
	FUNC(KeGetCurrentThread),                  // 0x0068 (104)
	FUNC(KeInitializeApc),                     // 0x0069 (105)
	FUNC(KeInitializeDeviceQueue),             // 0x006A (106)
	FUNC(KeInitializeDpc),                     // 0x006B (107)
	FUNC(KeInitializeEvent),                   // 0x006C (108)
	FUNC(KeInitializeInterrupt),               // 0x006D (109)
	FUNC(KeInitializeMutant),                  // 0x006E (110)
	FUNC(KeInitializeQueue),                   // 0x006F (111)
	FUNC(KeInitializeSemaphore),               // 0x0070 (112)
	FUNC(KeInitializeTimerEx),                 // 0x0071 (113)
	FUNC(KeInsertByKeyDeviceQueue),            // 0x0072 (114)
	FUNC(KeInsertDeviceQueue),                 // 0x0073 (115)
	FUNC(KeInsertHeadQueue),                   // 0x0074 (116)
	FUNC(KeInsertQueue),                       // 0x0075 (117)
	FUNC(KeInsertQueueApc),                    // 0x0076 (118)
	FUNC(KeInsertQueueDpc),                    // 0x0077 (119)
	PANIC(0x0078),                             // 0x0078 (120) VARIABLE(KeInterruptTime) // Set by ConnectWindowsTimersToThunkTable
	FUNC(KeIsExecutingDpc),                    // 0x0079 (121)
	FUNC(KeLeaveCriticalRegion),               // 0x007A (122)
	FUNC(KePulseEvent),                        // 0x007B (123)
	FUNC(KeQueryBasePriorityThread),           // 0x007C (124)
	FUNC(KeQueryInterruptTime),                // 0x007D (125)
	FUNC(KeQueryPerformanceCounter),           // 0x007E (126)
	FUNC(KeQueryPerformanceFrequency),         // 0x007F (127)
	FUNC(KeQuerySystemTime),                   // 0x0080 (128)
	FUNC(KeRaiseIrqlToDpcLevel),               // 0x0081 (129)
	FUNC(KeRaiseIrqlToSynchLevel),             // 0x0082 (130)
	FUNC(KeReleaseMutant),                     // 0x0083 (131)
	FUNC(KeReleaseSemaphore),                  // 0x0084 (132)
	FUNC(KeRemoveByKeyDeviceQueue),            // 0x0085 (133)
	FUNC(KeRemoveDeviceQueue),                 // 0x0086 (134)
	FUNC(KeRemoveEntryDeviceQueue),            // 0x0087 (135)
	FUNC(KeRemoveQueue),                       // 0x0088 (136)
	FUNC(KeRemoveQueueDpc),                    // 0x0089 (137)
	FUNC(KeResetEvent),                        // 0x008A (138)
	FUNC(KeRestoreFloatingPointState),         // 0x008B (139)
	FUNC(KeResumeThread),                      // 0x008C (140)
	FUNC(KeRundownQueue),                      // 0x008D (141)
	FUNC(KeSaveFloatingPointState),            // 0x008E (142)
	FUNC(KeSetBasePriorityThread),             // 0x008F (143)
	FUNC(KeSetDisableBoostThread),             // 0x0090 (144)
	FUNC(KeSetEvent),                          // 0x0091 (145)
	FUNC(KeSetEventBoostPriority),             // 0x0092 (146)
	FUNC(KeSetPriorityProcess),                // 0x0093 (147)
	FUNC(KeSetPriorityThread),                 // 0x0094 (148)
	FUNC(KeSetTimer),                          // 0x0095 (149)
	FUNC(KeSetTimerEx),                        // 0x0096 (150)
	FUNC(KeStallExecutionProcessor),           // 0x0097 (151)
	FUNC(KeSuspendThread),                     // 0x0098 (152)
	FUNC(KeSynchronizeExecution),              // 0x0099 (153)
	PANIC(0x009A),                             // 0x009A (154) VARIABLE(KeSystemTime) // Set by ConnectWindowsTimersToThunkTable
	FUNC(KeTestAlertThread),                   // 0x009B (155)
	VARIABLE(KeTickCount),                     // 0x009C (156)
	VARIABLE(KeTimeIncrement),                 // 0x009D (157)
	FUNC(KeWaitForMultipleObjects),            // 0x009E (158)
	FUNC(KeWaitForSingleObject),               // 0x009F (159)
	FUNC(KfRaiseIrql),                         // 0x00A0 (160)
	FUNC(KfLowerIrql),                         // 0x00A1 (161)
	VARIABLE(KiBugCheckData),                  // 0x00A2 (162)
	FUNC(KiUnlockDispatcherDatabase),          // 0x00A3 (163)
	VARIABLE(LaunchDataPage),                  // 0x00A4 (164)
	FUNC(MmAllocateContiguousMemory),          // 0x00A5 (165)
	FUNC(MmAllocateContiguousMemoryEx),        // 0x00A6 (166)
	FUNC(MmAllocateSystemMemory),              // 0x00A7 (167)
	FUNC(MmClaimGpuInstanceMemory),            // 0x00A8 (168)
	FUNC(MmCreateKernelStack),                 // 0x00A9 (169)
	FUNC(MmDeleteKernelStack),                 // 0x00AA (170)
	FUNC(MmFreeContiguousMemory),              // 0x00AB (171)
	FUNC(MmFreeSystemMemory),                  // 0x00AC (172)
	FUNC(MmGetPhysicalAddress),                // 0x00AD (173)
	FUNC(MmIsAddressValid),                    // 0x00AE (174)
	FUNC(MmLockUnlockBufferPages),             // 0x00AF (175)
	FUNC(MmLockUnlockPhysicalPage),            // 0x00B0 (176)
	FUNC(MmMapIoSpace),                        // 0x00B1 (177)
	FUNC(MmPersistContiguousMemory),           // 0x00B2 (178)
	FUNC(MmQueryAddressProtect),               // 0x00B3 (179)
	FUNC(MmQueryAllocationSize),               // 0x00B4 (180)
	FUNC(MmQueryStatistics),                   // 0x00B5 (181)
	FUNC(MmSetAddressProtect),                 // 0x00B6 (182)
	FUNC(MmUnmapIoSpace),                      // 0x00B7 (183)
	FUNC(NtAllocateVirtualMemory),             // 0x00B8 (184)
	FUNC(NtCancelTimer),                       // 0x00B9 (185)
	FUNC(NtClearEvent),                        // 0x00BA (186)
	FUNC(NtClose),                             // 0x00BB (187)
	FUNC(NtCreateDirectoryObject),             // 0x00BC (188)
	FUNC(NtCreateEvent),                       // 0x00BD (189)
	FUNC(NtCreateFile),                        // 0x00BE (190)
	FUNC(NtCreateIoCompletion),                // 0x00BF (191)
	FUNC(NtCreateMutant),                      // 0x00C0 (192)
	FUNC(NtCreateSemaphore),                   // 0x00C1 (193)
	FUNC(NtCreateTimer),                       // 0x00C2 (194)
	FUNC(NtDeleteFile),                        // 0x00C3 (195)
	FUNC(NtDeviceIoControlFile),               // 0x00C4 (196)
	FUNC(NtDuplicateObject),                   // 0x00C5 (197)
	FUNC(NtFlushBuffersFile),                  // 0x00C6 (198)
	FUNC(NtFreeVirtualMemory),                 // 0x00C7 (199)
	FUNC(NtFsControlFile),                     // 0x00C8 (200)
	FUNC(NtOpenDirectoryObject),               // 0x00C9 (201)
	FUNC(NtOpenFile),                          // 0x00CA (202)
	FUNC(NtOpenSymbolicLinkObject),            // 0x00CB (203)
	FUNC(NtProtectVirtualMemory),              // 0x00CC (204)
	FUNC(NtPulseEvent),                        // 0x00CD (205)
	FUNC(NtQueueApcThread),                    // 0x00CE (206)
	FUNC(NtQueryDirectoryFile),                // 0x00CF (207)
	PANIC(0x00D0),                             // 0x00D0 (208) TODO : FUNC(NtQueryDirectoryObject)
	FUNC(NtQueryEvent),                        // 0x00D1 (209)
	FUNC(NtQueryFullAttributesFile),           // 0x00D2 (210)
	FUNC(NtQueryInformationFile),              // 0x00D3 (211)
	PANIC(0x00D4),                             // 0x00D4 (212) TODO : FUNC(NtQueryIoCompletion)
	FUNC(NtQueryMutant),                       // 0x00D5 (213)
	FUNC(NtQuerySemaphore),                    // 0x00D6 (214)
	FUNC(NtQuerySymbolicLinkObject),           // 0x00D7 (215)
	FUNC(NtQueryTimer),                        // 0x00D8 (216)
	FUNC(NtQueryVirtualMemory),                // 0x00D9 (217)
	FUNC(NtQueryVolumeInformationFile),        // 0x00DA (218)
	FUNC(NtReadFile),                          // 0x00DB (219)
	PANIC(0x00DC),                             // 0x00DC (220) TODO : FUNC(NtReadFileScatter)
	FUNC(NtReleaseMutant),                     // 0x00DD (221)
	FUNC(NtReleaseSemaphore),                  // 0x00DE (222)
	PANIC(0x00DF),                             // 0x00DF (223) TODO : FUNC(NtRemoveIoCompletion)
	FUNC(NtResumeThread),                      // 0x00E0 (224)
	FUNC(NtSetEvent),                          // 0x00E1 (225)
	FUNC(NtSetInformationFile),                // 0x00E2 (226)
	PANIC(0x00E3),                             // 0x00E3 (227) TODO : FUNC(NtSetIoCompletion)
	FUNC(NtSetSystemTime),                     // 0x00E4 (228)
	FUNC(NtSetTimerEx),                        // 0x00E5 (229)
	PANIC(0x00E6),                             // 0x00E6 (230) TODO : FUNC(NtSignalAndWaitForSingleObjectEx)
	FUNC(NtSuspendThread),                     // 0x00E7 (231)
	FUNC(NtUserIoApcDispatcher),               // 0x00E8 (232)
	FUNC(NtWaitForSingleObject),               // 0x00E9 (233)
	FUNC(NtWaitForSingleObjectEx),             // 0x00EA (234)
	FUNC(NtWaitForMultipleObjectsEx),          // 0x00EB (235)
	FUNC(NtWriteFile),                         // 0x00EC (236)
	PANIC(0x00ED),                             // 0x00ED (237) TODO : FUNC(NtWriteFileGather)
	FUNC(NtYieldExecution),                    // 0x00EE (238)
	FUNC(ObCreateObject),                      // 0x00EF (239)
	VARIABLE(ObDirectoryObjectType),           // 0x00F0 (240)
	FUNC(ObInsertObject),                      // 0x00F1 (241)
	FUNC(ObMakeTemporaryObject),               // 0x00F2 (242) 
	FUNC(ObOpenObjectByName),                  // 0x00F3 (243)
	FUNC(ObOpenObjectByPointer),               // 0x00F4 (244)
	VARIABLE(ObpObjectHandleTable),            // 0x00F5 (245)
	FUNC(ObReferenceObjectByHandle),           // 0x00F6 (246)
	FUNC(ObReferenceObjectByName),             // 0x00F7 (247)
	FUNC(ObReferenceObjectByPointer),          // 0x00F8 (248)
	VARIABLE(ObSymbolicLinkObjectType),        // 0x00F9 (249)
	FUNC(ObfDereferenceObject),                // 0x00FA (250)
	FUNC(ObfReferenceObject),                  // 0x00FB (251)
	FUNC(PhyGetLinkState),                     // 0x00FC (252)
	FUNC(PhyInitialize),                       // 0x00FD (253)
	FUNC(PsCreateSystemThread),                // 0x00FE (254)
	FUNC(PsCreateSystemThreadEx),              // 0x00FF (255)
	FUNC(PsQueryStatistics),                   // 0x0100 (256)
	FUNC(PsSetCreateThreadNotifyRoutine),      // 0x0101 (257)
	FUNC(PsTerminateSystemThread),             // 0x0102 (258)
	VARIABLE(PsThreadObjectType),              // 0x0103 (259)
	FUNC(RtlAnsiStringToUnicodeString),        // 0x0104 (260)
	FUNC(RtlAppendStringToString),             // 0x0105 (261)
	FUNC(RtlAppendUnicodeStringToString),      // 0x0106 (262)
	FUNC(RtlAppendUnicodeToString),            // 0x0107 (263)
	FUNC(RtlAssert),                           // 0x0108 (264)
	PANIC(0x0109),                             // 0x0109 (265) TODO : FUNC(RtlCaptureContext)
	PANIC(0x010A),                             // 0x010A (266) TODO : FUNC(RtlCaptureStackBackTrace)
	FUNC(RtlCharToInteger),                    // 0x010B (267)
	FUNC(RtlCompareMemory),                    // 0x010C (268)
	FUNC(RtlCompareMemoryUlong),               // 0x010D (269)
	FUNC(RtlCompareString),                    // 0x010E (270)
	FUNC(RtlCompareUnicodeString),             // 0x010F (271)
	FUNC(RtlCopyString),                       // 0x0110 (272)
	FUNC(RtlCopyUnicodeString),                // 0x0111 (273)
	FUNC(RtlCreateUnicodeString),              // 0x0112 (274)
	FUNC(RtlDowncaseUnicodeChar),              // 0x0113 (275)
	FUNC(RtlDowncaseUnicodeString),            // 0x0114 (276)
	FUNC(RtlEnterCriticalSection),             // 0x0115 (277)
	FUNC(RtlEnterCriticalSectionAndRegion),    // 0x0116 (278)
	FUNC(RtlEqualString),                      // 0x0117 (279)
	FUNC(RtlEqualUnicodeString),               // 0x0118 (280)
	FUNC(RtlExtendedIntegerMultiply),          // 0x0119 (281)
	FUNC(RtlExtendedLargeIntegerDivide),       // 0x011A (282)
	FUNC(RtlExtendedMagicDivide),              // 0x011B (283)
	FUNC(RtlFillMemory),                       // 0x011C (284)
	FUNC(RtlFillMemoryUlong),                  // 0x011D (285)
	FUNC(RtlFreeAnsiString),                   // 0x011E (286)
	FUNC(RtlFreeUnicodeString),                // 0x011F (287)
	PANIC(0x0120),                             // 0x0120 (288) TODO : FUNC(RtlGetCallersAddress)
	FUNC(RtlInitAnsiString),                   // 0x0121 (289)
	FUNC(RtlInitUnicodeString),                // 0x0122 (290)
	FUNC(RtlInitializeCriticalSection),        // 0x0123 (291)
	FUNC(RtlIntegerToChar),                    // 0x0124 (292)
	FUNC(RtlIntegerToUnicodeString),           // 0x0125 (293)
	FUNC(RtlLeaveCriticalSection),             // 0x0126 (294)
	FUNC(RtlLeaveCriticalSectionAndRegion),    // 0x0127 (295)
	FUNC(RtlLowerChar),                        // 0x0128 (296)
	FUNC(RtlMapGenericMask),                   // 0x0129 (297)
	FUNC(RtlMoveMemory),                       // 0x012A (298)
	FUNC(RtlMultiByteToUnicodeN),              // 0x012B (299)
	FUNC(RtlMultiByteToUnicodeSize),           // 0x012C (300)
	FUNC(RtlNtStatusToDosError),               // 0x012D (301)
	PANIC(0x012E),                             // 0x012E (302) TODO : FUNC(RtlRaiseException)
	PANIC(0x012F),                             // 0x012F (303) TODO : FUNC(RtlRaiseStatus)
	FUNC(RtlTimeFieldsToTime),                 // 0x0130 (304)
	FUNC(RtlTimeToTimeFields),                 // 0x0131 (305)
	FUNC(RtlTryEnterCriticalSection),          // 0x0132 (306)
	FUNC(RtlUlongByteSwap),                    // 0x0133 (307)
	FUNC(RtlUnicodeStringToAnsiString),        // 0x0134 (308)
	FUNC(RtlUnicodeStringToInteger),           // 0x0135 (309)
	FUNC(RtlUnicodeToMultiByteN),              // 0x0136 (310)
	FUNC(RtlUnicodeToMultiByteSize),           // 0x0137 (311)
	PANIC(0x0138),                             // 0x0138 (312) TODO : FUNC(RtlUnwind)
	FUNC(RtlUpcaseUnicodeChar),                // 0x0139 (313)
	FUNC(RtlUpcaseUnicodeString),              // 0x013A (314)
	FUNC(RtlUpcaseUnicodeToMultiByteN),        // 0x013B (315)
	FUNC(RtlUpperChar),                        // 0x013C (316)
	FUNC(RtlUpperString),                      // 0x013D (317)
	FUNC(RtlUshortByteSwap),                   // 0x013E (318)
	PANIC(0x013F),                             // 0x013F (319) TODO : FUNC(RtlWalkFrameChain)
	FUNC(RtlZeroMemory),                       // 0x0140 (320)
	VARIABLE(XboxEEPROMKey),                   // 0x0141 (321)
	VARIABLE(XboxHardwareInfo),                // 0x0142 (322)
	VARIABLE(XboxHDKey),                       // 0x0143 (323)
	VARIABLE(XboxKrnlVersion),                 // 0x0144 (324)
	VARIABLE(XboxSignatureKey),                // 0x0145 (325)
	VARIABLE(XeImageFileName),                 // 0x0146 (326)
	FUNC(XeLoadSection),                       // 0x0147 (327)
	FUNC(XeUnloadSection),                     // 0x0148 (328)
	FUNC(READ_PORT_BUFFER_UCHAR),              // 0x0149 (329)
	FUNC(READ_PORT_BUFFER_USHORT),             // 0x014A (330)
	FUNC(READ_PORT_BUFFER_ULONG),              // 0x014B (331)
	FUNC(WRITE_PORT_BUFFER_UCHAR),             // 0x014C (332)
	FUNC(WRITE_PORT_BUFFER_USHORT),            // 0x014D (333)
	FUNC(WRITE_PORT_BUFFER_ULONG),             // 0x014E (334)
	FUNC(XcSHAInit),                           // 0x014F (335)
	FUNC(XcSHAUpdate),                         // 0x0150 (336)
	FUNC(XcSHAFinal),                          // 0x0151 (337)
	FUNC(XcRC4Key),                            // 0x0152 (338)
	FUNC(XcRC4Crypt),                          // 0x0153 (339)
	FUNC(XcHMAC),                              // 0x0154 (340)
	FUNC(XcPKEncPublic),                       // 0x0155 (341)
	FUNC(XcPKDecPrivate),                      // 0x0156 (342)
	FUNC(XcPKGetKeyLen),                       // 0x0157 (343)
	FUNC(XcVerifyPKCS1Signature),              // 0x0158 (344)
	FUNC(XcModExp),                            // 0x0159 (345)
	FUNC(XcDESKeyParity),                      // 0x015A (346)
	FUNC(XcKeyTable),                          // 0x015B (347)
	FUNC(XcBlockCrypt),                        // 0x015C (348)
	FUNC(XcBlockCryptCBC),                     // 0x015D (349)
	FUNC(XcCryptService),                      // 0x015E (350)
	FUNC(XcUpdateCrypto),                      // 0x015F (351)
	FUNC(RtlRip),                              // 0x0160 (352)
	VARIABLE(XboxLANKey),                      // 0x0161 (353)
	VARIABLE(XboxAlternateSignatureKeys),      // 0x0162 (354)
	VARIABLE(XePublicKeyData),                 // 0x0163 (355)
	VARIABLE(HalBootSMCVideoMode),             // 0x0164 (356)
	VARIABLE(IdexChannelObject),               // 0x0165 (357)
	FUNC(HalIsResetOrShutdownPending),         // 0x0166 (358)
	FUNC(IoMarkIrpMustComplete),               // 0x0167 (359)
	FUNC(HalInitiateShutdown),                 // 0x0168 (360)
	PANIC(0x0169),                             // 0x0169 (361) TODO : FUNC(KRNL(_snprintf))
	PANIC(0x016A),                             // 0x016A (362) TODO : FUNC(KRNL(_sprintf))
	PANIC(0x016B),                             // 0x016B (363) TODO : FUNC(KRNL(_vsnprintf))
	PANIC(0x016C),                             // 0x016C (364) TODO : FUNC(KRNL(_vsprintf))
	FUNC(HalEnableSecureTrayEject),            // 0x016D (365)
	FUNC(HalWriteSMCScratchRegister),          // 0x016E (366)
	FUNC(UnknownAPI367),                       // 0x016F (367)
	FUNC(UnknownAPI368),                       // 0x0170 (368)
	FUNC(UnknownAPI369),                       // 0x0171 (369)
	FUNC(XProfpControl),                       // 0x0172 (370) PROFILING
	FUNC(XProfpGetData),                       // 0x0173 (371) PROFILING
	FUNC(IrtClientInitFast),                   // 0x0174 (372) PROFILING
	FUNC(IrtSweep),                            // 0x0175 (373) PROFILING
	PANIC(0x0176),                             // 0x0177 (374) DEVKIT TODO : FUNC(MmDbgAllocateMemory)
	PANIC(0x0177),                             // 0x0178 (375) DEVKIT TODO : FUNC(MmDbgFreeMemory) // Returns number of pages released.
	PANIC(0x0178),                             // 0x0179 (376) DEVKIT TODO : FUNC(MmDbgQueryAvailablePages)
	PANIC(0x0179),                             // 0x017A (377) DEVKIT TODO : FUNC(MmDbgReleaseAddress)
	PANIC(0x017A),                             // 0x017A (378) DEVKIT TODO : FUNC(MmDbgWriteCheck)
};

/* prevent name collisions */
namespace NtDll
{
	#include "EmuNtDll.h"
};

// Virtual memory location of KUSER_SHARED_DATA :
// See http://research.microsoft.com/en-us/um/redmond/projects/invisible/src/base/md/i386/sim/_pertest2.c.htm
// and http://research.microsoft.com/en-us/um/redmond/projects/invisible/src/base/md/i386/sim/_glue.c.htm
// and http://processhacker.sourceforge.net/doc/ntexapi_8h_source.html
// and http://forum.sysinternals.com/0x7ffe0000-what-is-in-it_topic10012.html
#define MM_SHARED_USER_DATA_VA      0x7FFE0000
#define USER_SHARED_DATA ((NtDll::KUSER_SHARED_DATA * const)MM_SHARED_USER_DATA_VA)

// KUSER_SHARED_DATA Offsets
// See http://native-nt-toolkit.googlecode.com/svn/trunk/ndk/asm.h
// Note : KUSER_SHARED_DATA.TickCountLow seems deprecated
const UINT USER_SHARED_DATA_TICK_COUNT = 0x320;

// Here we define the addresses of the native Windows timers :
// Source: Dxbx
const xboxkrnl::PKSYSTEM_TIME CxbxNtTickCount = (xboxkrnl::PKSYSTEM_TIME)(MM_SHARED_USER_DATA_VA + USER_SHARED_DATA_TICK_COUNT);

void ConnectWindowsTimersToThunkTable()
{
	// Couple the xbox thunks for xboxkrnl::KeInterruptTime and xboxkrnl::KeSystemTime
	// to their actual counterparts on Windows, this way we won't have to spend any
	// time on updating them ourselves, and still get highly accurate timers!
	// See http://www.dcl.hpi.uni-potsdam.de/research/WRK/2007/08/getting-os-information-the-kuser_shared_data-structure/

	// Point Xbox KeInterruptTime to host InterruptTime:
	CxbxKrnl_KernelThunkTable[120] = (uint32)&(USER_SHARED_DATA->InterruptTime);

	// Point Xbox KeSystemTime to host SystemTime; If read directly (thus skipping
	// KeQuerySystemTime), this value is not adjusted with HostSystemTimeDelta!
	CxbxKrnl_KernelThunkTable[154] = (uint32)&(USER_SHARED_DATA->SystemTime);

	// We can't point Xbox KeTickCount to host TickCount, because it
	// updates slower on the xbox. See EmuUpdateTickCount().
}
