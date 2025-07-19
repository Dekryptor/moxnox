.intel_syntax noprefix

#include "dwarf.S"

.section .debug_abbrev, "dr"

.uleb128 1 # entry num
.uleb128 DW_TAG_compile_unit
.byte 1 # has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 # entry num
.uleb128 DW_TAG_subprogram
.byte 0 # has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 # version
.byte DW_UT_compile
.byte 8 # address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text # low_pc
.uleb128 .text_end - .text # high_pc
.asciz __FILE__ # name
.short DW_LANG_Mips_Assembler # language

dwarf_func_asm NtDisplayString
dwarf_func_asm NtTerminateThread
dwarf_func_asm NtTerminateProcess
dwarf_func_asm NtWriteFile
dwarf_func_asm NtResumeProcess
dwarf_func_asm NtAllocateVirtualMemory
dwarf_func_asm NtFreeVirtualMemory
dwarf_func_asm NtReadFile
dwarf_func_asm NtProtectVirtualMemory
dwarf_func_asm NtAcceptConnectPort
dwarf_func_asm NtAccessCheck
dwarf_func_asm NtAccessCheckAndAuditAlarm
dwarf_func_asm NtAddAtom
dwarf_func_asm NtAdjustGroupsToken
dwarf_func_asm NtAdjustPrivilegesToken
dwarf_func_asm NtAlertResumeThread
dwarf_func_asm NtAlertThread
dwarf_func_asm NtAllocateLocallyUniqueId
dwarf_func_asm NtAllocateUuids
dwarf_func_asm NtCallbackReturn
dwarf_func_asm NtCancelIoFile
dwarf_func_asm NtCancelTimer
dwarf_func_asm NtClearEvent
dwarf_func_asm NtClose
dwarf_func_asm NtCloseObjectAuditAlarm
dwarf_func_asm NtCompleteConnectPort
dwarf_func_asm NtConnectPort
dwarf_func_asm NtCreateDirectoryObject
dwarf_func_asm NtCreateEvent
dwarf_func_asm NtCreateEventPair
dwarf_func_asm NtCreateFile
dwarf_func_asm NtCreateIoCompletion
dwarf_func_asm NtCreateKey
dwarf_func_asm NtCreateMailslotFile
dwarf_func_asm NtCreateMutant
dwarf_func_asm NtCreateNamedPipeFile
dwarf_func_asm NtCreatePagingFile
dwarf_func_asm NtCreatePort
dwarf_func_asm NtCreateProcess
dwarf_func_asm NtCreateProfile
dwarf_func_asm NtCreateSection
dwarf_func_asm NtCreateSemaphore
dwarf_func_asm NtCreateSymbolicLinkObject
dwarf_func_asm NtCreateThread
dwarf_func_asm NtCreateTimer
dwarf_func_asm NtCreateToken
dwarf_func_asm NtDelayExecution
dwarf_func_asm NtDeleteAtom
dwarf_func_asm NtDeleteFile
dwarf_func_asm NtDeleteKey
dwarf_func_asm NtDeleteObjectAuditAlarm
dwarf_func_asm NtDeleteValueKey
dwarf_func_asm NtDeviceIoControlFile
dwarf_func_asm NtDuplicateObject
dwarf_func_asm NtDuplicateToken
dwarf_func_asm NtEnumerateKey
dwarf_func_asm NtEnumerateValueKey
dwarf_func_asm NtExtendSection
dwarf_func_asm NtFindAtom
dwarf_func_asm NtFlushBuffersFile
dwarf_func_asm NtFlushInstructionCache
dwarf_func_asm NtFlushKey
dwarf_func_asm NtFlushVirtualMemory
dwarf_func_asm NtFlushWriteBuffer
dwarf_func_asm NtFsControlFile
dwarf_func_asm NtGetContextThread
dwarf_func_asm NtImpersonateClientOfPort
dwarf_func_asm NtImpersonateThread
dwarf_func_asm NtInitializeRegistry
dwarf_func_asm NtListenPort
dwarf_func_asm NtLoadDriver
dwarf_func_asm NtLoadKey
dwarf_func_asm NtLoadKey2
dwarf_func_asm NtLockFile
dwarf_func_asm NtLockVirtualMemory
dwarf_func_asm NtMakeTemporaryObject
dwarf_func_asm NtMapViewOfSection
dwarf_func_asm NtNotifyChangeDirectoryFile
dwarf_func_asm NtNotifyChangeKey
dwarf_func_asm NtOpenDirectoryObject
dwarf_func_asm NtOpenEvent
dwarf_func_asm NtOpenEventPair
dwarf_func_asm NtOpenFile
dwarf_func_asm NtOpenIoCompletion
dwarf_func_asm NtOpenKey
dwarf_func_asm NtOpenMutant
dwarf_func_asm NtOpenObjectAuditAlarm
dwarf_func_asm NtOpenProcess
dwarf_func_asm NtOpenProcessToken
dwarf_func_asm NtOpenSection
dwarf_func_asm NtOpenSemaphore
dwarf_func_asm NtOpenSymbolicLinkObject
dwarf_func_asm NtOpenThread
dwarf_func_asm NtOpenThreadToken
dwarf_func_asm NtOpenTimer
dwarf_func_asm NtPlugPlayControl
dwarf_func_asm NtPrivilegeCheck
dwarf_func_asm NtPrivilegeObjectAuditAlarm
dwarf_func_asm NtPrivilegedServiceAuditAlarm
dwarf_func_asm NtPulseEvent
dwarf_func_asm NtQueryAttributesFile
dwarf_func_asm NtQueryDefaultLocale
dwarf_func_asm NtQueryDirectoryFile
dwarf_func_asm NtQueryDirectoryObject
dwarf_func_asm NtQueryEaFile
dwarf_func_asm NtQueryEvent
dwarf_func_asm NtQueryFullAttributesFile
dwarf_func_asm NtQueryInformationAtom
dwarf_func_asm NtQueryInformationFile
dwarf_func_asm NtQueryInformationPort
dwarf_func_asm NtQueryInformationProcess
dwarf_func_asm NtQueryInformationThread
dwarf_func_asm NtQueryInformationToken
dwarf_func_asm NtQueryIntervalProfile
dwarf_func_asm NtQueryIoCompletion
dwarf_func_asm NtQueryKey
dwarf_func_asm NtQueryMultipleValueKey
dwarf_func_asm NtQueryMutant
dwarf_func_asm NtQueryObject
dwarf_func_asm NtQueryPerformanceCounter
dwarf_func_asm NtQuerySection
dwarf_func_asm NtQuerySecurityObject
dwarf_func_asm NtQuerySemaphore
dwarf_func_asm NtQuerySymbolicLinkObject
dwarf_func_asm NtQuerySystemEnvironmentValue
dwarf_func_asm NtQuerySystemInformation
dwarf_func_asm NtQuerySystemTime
dwarf_func_asm NtQueryTimer
dwarf_func_asm NtQueryTimerResolution
dwarf_func_asm NtQueryValueKey
dwarf_func_asm NtQueryVirtualMemory
dwarf_func_asm NtQueryVolumeInformationFile
dwarf_func_asm NtQueueApcThread
dwarf_func_asm NtRaiseException
dwarf_func_asm NtRaiseHardError
dwarf_func_asm NtReadFileScatter
dwarf_func_asm NtReadRequestData
dwarf_func_asm NtReadVirtualMemory
dwarf_func_asm NtRegisterThreadTerminatePort
dwarf_func_asm NtReleaseMutant
dwarf_func_asm NtReleaseSemaphore
dwarf_func_asm NtRemoveIoCompletion
dwarf_func_asm NtReplaceKey
dwarf_func_asm NtReplyPort
dwarf_func_asm NtReplyWaitReceivePort
dwarf_func_asm NtReplyWaitReplyPort
dwarf_func_asm NtRequestPort
dwarf_func_asm NtRequestWaitReplyPort
dwarf_func_asm NtResetEvent
dwarf_func_asm NtRestoreKey
dwarf_func_asm NtResumeThread
dwarf_func_asm NtSaveKey
dwarf_func_asm NtSetContextThread
dwarf_func_asm NtSetDefaultHardErrorPort
dwarf_func_asm NtSetDefaultLocale
dwarf_func_asm NtSetEaFile
dwarf_func_asm NtSetEvent
dwarf_func_asm NtSetHighEventPair
dwarf_func_asm NtSetHighWaitLowEventPair
dwarf_func_asm NtSetInformationFile
dwarf_func_asm NtSetInformationKey
dwarf_func_asm NtSetInformationObject
dwarf_func_asm NtSetInformationProcess
dwarf_func_asm NtSetInformationThread
dwarf_func_asm NtSetInformationToken
dwarf_func_asm NtSetIntervalProfile
dwarf_func_asm NtSetIoCompletion
dwarf_func_asm NtSetLdtEntries
dwarf_func_asm NtSetLowEventPair
dwarf_func_asm NtSetLowWaitHighEventPair
dwarf_func_asm NtSetSecurityObject
dwarf_func_asm NtSetSystemEnvironmentValue
dwarf_func_asm NtSetSystemInformation
dwarf_func_asm NtSetSystemPowerState
dwarf_func_asm NtSetSystemTime
dwarf_func_asm NtSetTimer
dwarf_func_asm NtSetTimerResolution
dwarf_func_asm NtSetValueKey
dwarf_func_asm NtSetVolumeInformationFile
dwarf_func_asm NtShutdownSystem
dwarf_func_asm NtSignalAndWaitForSingleObject
dwarf_func_asm NtStartProfile
dwarf_func_asm NtStopProfile
dwarf_func_asm NtSuspendThread
dwarf_func_asm NtSystemDebugControl
dwarf_func_asm NtTestAlert
dwarf_func_asm NtUnloadDriver
dwarf_func_asm NtUnloadKey
dwarf_func_asm NtUnlockFile
dwarf_func_asm NtUnlockVirtualMemory
dwarf_func_asm NtUnmapViewOfSection
dwarf_func_asm NtVdmControl
dwarf_func_asm NtWaitForMultipleObjects
dwarf_func_asm NtWaitForSingleObject
dwarf_func_asm NtWaitHighEventPair
dwarf_func_asm NtWaitLowEventPair
dwarf_func_asm NtWriteFileGather
dwarf_func_asm NtWriteRequestData
dwarf_func_asm NtWriteVirtualMemory
dwarf_func_asm NtYieldExecution
dwarf_func_asm NtAccessCheckByType
dwarf_func_asm NtAccessCheckByTypeAndAuditAlarm
dwarf_func_asm NtAccessCheckByTypeResultList
dwarf_func_asm NtAccessCheckByTypeResultListAndAuditAlarm
dwarf_func_asm NtAccessCheckByTypeResultListAndAuditAlarmByHandle
dwarf_func_asm NtAcquireCMFViewOwnership
dwarf_func_asm NtAddBootEntry
dwarf_func_asm NtAddDriverEntry
dwarf_func_asm NtAllocateUserPhysicalPages
dwarf_func_asm NtAlpcAcceptConnectPort
dwarf_func_asm NtAlpcCancelMessage
dwarf_func_asm NtAlpcConnectPort
dwarf_func_asm NtAlpcCreatePort
dwarf_func_asm NtAlpcCreatePortSection
dwarf_func_asm NtAlpcCreateResourceReserve
dwarf_func_asm NtAlpcCreateSectionView
dwarf_func_asm NtAlpcCreateSecurityContext
dwarf_func_asm NtAlpcDeletePortSection
dwarf_func_asm NtAlpcDeleteResourceReserve
dwarf_func_asm NtAlpcDeleteSectionView
dwarf_func_asm NtAlpcDeleteSecurityContext
dwarf_func_asm NtAlpcDisconnectPort
dwarf_func_asm NtAlpcImpersonateClientOfPort
dwarf_func_asm NtAlpcOpenSenderProcess
dwarf_func_asm NtAlpcOpenSenderThread
dwarf_func_asm NtAlpcQueryInformation
dwarf_func_asm NtAlpcQueryInformationMessage
dwarf_func_asm NtAlpcRevokeSecurityContext
dwarf_func_asm NtAlpcSendWaitReceivePort
dwarf_func_asm NtAlpcSetInformation
dwarf_func_asm NtApphelpCacheControl
dwarf_func_asm NtAreMappedFilesTheSame
dwarf_func_asm NtAssignProcessToJobObject
dwarf_func_asm NtCancelDeviceWakeupRequest
dwarf_func_asm NtCancelIoFileEx
dwarf_func_asm NtCancelSynchronousIoFile
dwarf_func_asm NtClearAllSavepointsTransaction
dwarf_func_asm NtClearSavepointTransaction
dwarf_func_asm NtCommitComplete
dwarf_func_asm NtCommitEnlistment
dwarf_func_asm NtCommitTransaction
dwarf_func_asm NtCompactKeys
dwarf_func_asm NtCompareTokens
dwarf_func_asm NtCompressKey
dwarf_func_asm NtCreateDebugObject
dwarf_func_asm NtCreateEnlistment
dwarf_func_asm NtCreateJobObject
dwarf_func_asm NtCreateJobSet
dwarf_func_asm NtCreateKeyedEvent
dwarf_func_asm NtCreateKeyTransacted
dwarf_func_asm NtCreatePrivateNamespace
dwarf_func_asm NtCreateProcessEx
dwarf_func_asm NtCreateResourceManager
dwarf_func_asm NtCreateThreadEx
dwarf_func_asm NtCreateTransaction
dwarf_func_asm NtCreateTransactionManager
dwarf_func_asm NtCreateUserProcess
dwarf_func_asm NtCreateWaitablePort
dwarf_func_asm NtCreateWorkerFactory
dwarf_func_asm NtDebugActiveProcess
dwarf_func_asm NtDebugContinue
dwarf_func_asm NtDeleteBootEntry
dwarf_func_asm NtDeleteDriverEntry
dwarf_func_asm NtDeletePrivateNamespace
dwarf_func_asm NtEnumerateBootEntries
dwarf_func_asm NtEnumerateDriverEntries
dwarf_func_asm NtEnumerateSystemEnvironmentValuesEx
dwarf_func_asm NtEnumerateTransactionObject
dwarf_func_asm NtFilterToken
dwarf_func_asm NtFlushInstallUILanguage
dwarf_func_asm NtFlushProcessWriteBuffers
dwarf_func_asm NtFreeUserPhysicalPages
dwarf_func_asm NtFreezeRegistry
dwarf_func_asm NtFreezeTransactions
dwarf_func_asm NtGetCurrentProcessorNumber
dwarf_func_asm NtGetDevicePowerState
dwarf_func_asm NtGetMUIRegistryInfo
dwarf_func_asm NtGetNextProcess
dwarf_func_asm NtGetNextThread
dwarf_func_asm NtGetNlsSectionPtr
dwarf_func_asm NtGetNotificationResourceManager
dwarf_func_asm NtGetPlugPlayEvent
dwarf_func_asm NtGetWriteWatch
dwarf_func_asm NtImpersonateAnonymousToken
dwarf_func_asm NtInitializeNlsFiles
dwarf_func_asm NtInitiatePowerAction
dwarf_func_asm NtIsProcessInJob
dwarf_func_asm NtIsSystemResumeAutomatic
dwarf_func_asm NtIsUILanguageComitted
dwarf_func_asm NtListTransactions
dwarf_func_asm NtLoadKeyEx
dwarf_func_asm NtLockProductActivationKeys
dwarf_func_asm NtLockRegistryKey
dwarf_func_asm NtMakePermanentObject
dwarf_func_asm NtMapCMFModule
dwarf_func_asm NtMapUserPhysicalPages
dwarf_func_asm NtMapUserPhysicalPagesScatter
dwarf_func_asm NtMarshallTransaction
dwarf_func_asm NtModifyBootEntry
dwarf_func_asm NtModifyDriverEntry
dwarf_func_asm NtNotifyChangeMultipleKeys
dwarf_func_asm NtOpenEnlistment
dwarf_func_asm NtOpenJobObject
dwarf_func_asm NtOpenKeyedEvent
dwarf_func_asm NtOpenKeyTransacted
dwarf_func_asm NtOpenPrivateNamespace
dwarf_func_asm NtOpenProcessTokenEx
dwarf_func_asm NtOpenResourceManager
dwarf_func_asm NtOpenSession
dwarf_func_asm NtOpenThreadTokenEx
dwarf_func_asm NtOpenTransaction
dwarf_func_asm NtOpenTransactionManager
dwarf_func_asm NtPowerInformation
dwarf_func_asm NtPrepareComplete
dwarf_func_asm NtPrepareEnlistment
dwarf_func_asm NtPrePrepareComplete
dwarf_func_asm NtPrePrepareEnlistment
dwarf_func_asm NtPropagationComplete
dwarf_func_asm NtPropagationFailed
dwarf_func_asm NtPullTransaction
dwarf_func_asm NtQueryBootEntryOrder
dwarf_func_asm NtQueryBootOptions
dwarf_func_asm NtQueryDebugFilterState
dwarf_func_asm NtQueryDefaultUILanguage
dwarf_func_asm NtQueryDriverEntryOrder
dwarf_func_asm NtQueryInformationEnlistment
dwarf_func_asm NtQueryInformationJobObject
dwarf_func_asm NtQueryInformationResourceManager
dwarf_func_asm NtQueryInformationTransaction
dwarf_func_asm NtQueryInformationTransactionManager
dwarf_func_asm NtQueryInformationWorkerFactory
dwarf_func_asm NtQueryInstallUILanguage
dwarf_func_asm NtQueryLicenseValue
dwarf_func_asm NtQueryOpenSubKeys
dwarf_func_asm NtQueryOpenSubKeysEx
dwarf_func_asm NtQueryPortInformationProcess
dwarf_func_asm NtQueryQuotaInformationFile
dwarf_func_asm NtQuerySystemEnvironmentValueEx
dwarf_func_asm NtReadOnlyEnlistment
dwarf_func_asm NtRecoverEnlistment
dwarf_func_asm NtRecoverResourceManager
dwarf_func_asm NtRecoverTransactionManager
dwarf_func_asm NtRegisterProtocolAddressInformation
dwarf_func_asm NtReleaseCMFViewOwnership
dwarf_func_asm NtReleaseKeyedEvent
dwarf_func_asm NtReleaseWorkerFactoryWorker
dwarf_func_asm NtRemoveIoCompletionEx
dwarf_func_asm NtRemoveProcessDebug
dwarf_func_asm NtRenameKey
dwarf_func_asm NtRenameTransactionManager
dwarf_func_asm NtReplacePartitionUnit
dwarf_func_asm NtReplyWaitReceivePortEx
dwarf_func_asm NtRequestDeviceWakeup
dwarf_func_asm NtRequestWakeupLatency
dwarf_func_asm NtResetWriteWatch
dwarf_func_asm NtRollbackComplete
dwarf_func_asm NtRollbackEnlistment
dwarf_func_asm NtRollbackSavepointTransaction
dwarf_func_asm NtRollbackTransaction
dwarf_func_asm NtRollforwardTransactionManager
dwarf_func_asm NtSaveKeyEx
dwarf_func_asm NtSaveMergedKeys
dwarf_func_asm NtSavepointComplete
dwarf_func_asm NtSavepointTransaction
dwarf_func_asm NtSecureConnectPort
dwarf_func_asm NtSetBootEntryOrder
dwarf_func_asm NtSetBootOptions
dwarf_func_asm NtSetDebugFilterState
dwarf_func_asm NtSetDefaultUILanguage
dwarf_func_asm NtSetDriverEntryOrder
dwarf_func_asm NtSetEventBoostPriority
dwarf_func_asm NtSetInformationDebugObject
dwarf_func_asm NtSetInformationEnlistment
dwarf_func_asm NtSetInformationJobObject
dwarf_func_asm NtSetInformationResourceManager
dwarf_func_asm NtSetInformationTransaction
dwarf_func_asm NtSetInformationTransactionManager
dwarf_func_asm NtSetInformationWorkerFactory
dwarf_func_asm NtSetQuotaInformationFile
dwarf_func_asm NtSetSystemEnvironmentValueEx
dwarf_func_asm NtSetThreadExecutionState
dwarf_func_asm NtSetUuidSeed
dwarf_func_asm NtShutdownWorkerFactory
dwarf_func_asm NtSinglePhaseReject
dwarf_func_asm NtStartTm
dwarf_func_asm NtSuspendProcess
dwarf_func_asm NtTerminateJobObject
dwarf_func_asm NtThawRegistry
dwarf_func_asm NtThawTransactions
dwarf_func_asm NtTraceControl
dwarf_func_asm NtTraceEvent
dwarf_func_asm NtTranslateFilePath
dwarf_func_asm NtUnloadKey2
dwarf_func_asm NtUnloadKeyEx
dwarf_func_asm NtWaitForDebugEvent
dwarf_func_asm NtWaitForKeyedEvent
dwarf_func_asm NtWaitForMultipleObjects32
dwarf_func_asm NtWaitForWorkViaWorkerFactory
dwarf_func_asm NtWorkerFactoryWorkerReady

.uleb128 0

.debug_info_end:

.text

.macro do_syscall name, num
    .text
    .global "\name"
    "\name":
    mov r10, rcx
    mov eax, \num
    syscall
    ret
    \name\()_end:

    .section .drectve
    .ascii " -export:\"\name\""
.endm

do_syscall "NtDisplayString", 0x1000
do_syscall "NtTerminateThread", 0x1001
do_syscall "NtTerminateProcess", 0x1002
do_syscall "NtWriteFile", 0x1003
do_syscall "NtResumeProcess", 0x1004
do_syscall "NtAllocateVirtualMemory", 0x1005
do_syscall "NtFreeVirtualMemory", 0x1006
do_syscall "NtReadFile", 0x1007
do_syscall "NtProtectVirtualMemory", 0x1008
do_syscall "NtAcceptConnectPort", 0x1009
do_syscall "NtAccessCheck", 0x100a
do_syscall "NtAccessCheckAndAuditAlarm", 0x100b
do_syscall "NtAddAtom", 0x100c
do_syscall "NtAdjustGroupsToken", 0x100d
do_syscall "NtAdjustPrivilegesToken", 0x100e
do_syscall "NtAlertResumeThread", 0x100f
do_syscall "NtAlertThread", 0x1010
do_syscall "NtAllocateLocallyUniqueId", 0x1011
do_syscall "NtAllocateUuids", 0x1012
do_syscall "NtCallbackReturn", 0x1013
do_syscall "NtCancelIoFile", 0x1014
do_syscall "NtCancelTimer", 0x1015
do_syscall "NtClearEvent", 0x1016
do_syscall "NtClose", 0x1017
do_syscall "NtCloseObjectAuditAlarm", 0x1018
do_syscall "NtCompleteConnectPort", 0x1019
do_syscall "NtConnectPort", 0x101a
do_syscall "NtCreateDirectoryObject", 0x101b
do_syscall "NtCreateEvent", 0x101c
do_syscall "NtCreateEventPair", 0x101d
do_syscall "NtCreateFile", 0x101e
do_syscall "NtCreateIoCompletion", 0x101f
do_syscall "NtCreateKey", 0x1020
do_syscall "NtCreateMailslotFile", 0x1021
do_syscall "NtCreateMutant", 0x1022
do_syscall "NtCreateNamedPipeFile", 0x1023
do_syscall "NtCreatePagingFile", 0x1024
do_syscall "NtCreatePort", 0x1025
do_syscall "NtCreateProcess", 0x1026
do_syscall "NtCreateProfile", 0x1027
do_syscall "NtCreateSection", 0x1028
do_syscall "NtCreateSemaphore", 0x1029
do_syscall "NtCreateSymbolicLinkObject", 0x102a
do_syscall "NtCreateThread", 0x102b
do_syscall "NtCreateTimer", 0x102c
do_syscall "NtCreateToken", 0x102d
do_syscall "NtDelayExecution", 0x102e
do_syscall "NtDeleteAtom", 0x102f
do_syscall "NtDeleteFile", 0x1030
do_syscall "NtDeleteKey", 0x1031
do_syscall "NtDeleteObjectAuditAlarm", 0x1032
do_syscall "NtDeleteValueKey", 0x1033
do_syscall "NtDeviceIoControlFile", 0x1034
do_syscall "NtDuplicateObject", 0x1035
do_syscall "NtDuplicateToken", 0x1036
do_syscall "NtEnumerateKey", 0x1037
do_syscall "NtEnumerateValueKey", 0x1038
do_syscall "NtExtendSection", 0x1039
do_syscall "NtFindAtom", 0x103a
do_syscall "NtFlushBuffersFile", 0x103b
do_syscall "NtFlushInstructionCache", 0x103c
do_syscall "NtFlushKey", 0x103d
do_syscall "NtFlushVirtualMemory", 0x103e
do_syscall "NtFlushWriteBuffer", 0x103f
do_syscall "NtFsControlFile", 0x1040
do_syscall "NtGetContextThread", 0x1041
do_syscall "NtImpersonateClientOfPort", 0x1042
do_syscall "NtImpersonateThread", 0x1043
do_syscall "NtInitializeRegistry", 0x1044
do_syscall "NtListenPort", 0x1045
do_syscall "NtLoadDriver", 0x1046
do_syscall "NtLoadKey", 0x1047
do_syscall "NtLoadKey2", 0x1048
do_syscall "NtLockFile", 0x1049
do_syscall "NtLockVirtualMemory", 0x104a
do_syscall "NtMakeTemporaryObject", 0x104b
do_syscall "NtMapViewOfSection", 0x104c
do_syscall "NtNotifyChangeDirectoryFile", 0x104d
do_syscall "NtNotifyChangeKey", 0x104e
do_syscall "NtOpenDirectoryObject", 0x104f
do_syscall "NtOpenEvent", 0x1050
do_syscall "NtOpenEventPair", 0x1051
do_syscall "NtOpenFile", 0x1052
do_syscall "NtOpenIoCompletion", 0x1053
do_syscall "NtOpenKey", 0x1054
do_syscall "NtOpenMutant", 0x1055
do_syscall "NtOpenObjectAuditAlarm", 0x1056
do_syscall "NtOpenProcess", 0x1057
do_syscall "NtOpenProcessToken", 0x1058
do_syscall "NtOpenSection", 0x1059
do_syscall "NtOpenSemaphore", 0x105a
do_syscall "NtOpenSymbolicLinkObject", 0x105b
do_syscall "NtOpenThread", 0x105c
do_syscall "NtOpenThreadToken", 0x105d
do_syscall "NtOpenTimer", 0x105e
do_syscall "NtPlugPlayControl", 0x105f
do_syscall "NtPrivilegeCheck", 0x1060
do_syscall "NtPrivilegeObjectAuditAlarm", 0x1061
do_syscall "NtPrivilegedServiceAuditAlarm", 0x1062
do_syscall "NtPulseEvent", 0x1063
do_syscall "NtQueryAttributesFile", 0x1064
do_syscall "NtQueryDefaultLocale", 0x1065
do_syscall "NtQueryDirectoryFile", 0x1066
do_syscall "NtQueryDirectoryObject", 0x1067
do_syscall "NtQueryEaFile", 0x1068
do_syscall "NtQueryEvent", 0x1069
do_syscall "NtQueryFullAttributesFile", 0x106a
do_syscall "NtQueryInformationAtom", 0x106b
do_syscall "NtQueryInformationFile", 0x106c
do_syscall "NtQueryInformationPort", 0x106d
do_syscall "NtQueryInformationProcess", 0x106e
do_syscall "NtQueryInformationThread", 0x106f
do_syscall "NtQueryInformationToken", 0x1070
do_syscall "NtQueryIntervalProfile", 0x1071
do_syscall "NtQueryIoCompletion", 0x1072
do_syscall "NtQueryKey", 0x1073
do_syscall "NtQueryMultipleValueKey", 0x1074
do_syscall "NtQueryMutant", 0x1075
do_syscall "NtQueryObject", 0x1076
do_syscall "NtQueryPerformanceCounter", 0x1077
do_syscall "NtQuerySection", 0x1078
do_syscall "NtQuerySecurityObject", 0x1079
do_syscall "NtQuerySemaphore", 0x107a
do_syscall "NtQuerySymbolicLinkObject", 0x107b
do_syscall "NtQuerySystemEnvironmentValue", 0x107c
do_syscall "NtQuerySystemInformation", 0x107d
do_syscall "NtQuerySystemTime", 0x107e
do_syscall "NtQueryTimer", 0x107f
do_syscall "NtQueryTimerResolution", 0x1080
do_syscall "NtQueryValueKey", 0x1081
do_syscall "NtQueryVirtualMemory", 0x1082
do_syscall "NtQueryVolumeInformationFile", 0x1083
do_syscall "NtQueueApcThread", 0x1084
do_syscall "NtRaiseException", 0x1085
do_syscall "NtRaiseHardError", 0x1086
do_syscall "NtReadFileScatter", 0x1087
do_syscall "NtReadRequestData", 0x1088
do_syscall "NtReadVirtualMemory", 0x1089
do_syscall "NtRegisterThreadTerminatePort", 0x108a
do_syscall "NtReleaseMutant", 0x108b
do_syscall "NtReleaseSemaphore", 0x108c
do_syscall "NtRemoveIoCompletion", 0x108d
do_syscall "NtReplaceKey", 0x108e
do_syscall "NtReplyPort", 0x108f
do_syscall "NtReplyWaitReceivePort", 0x1090
do_syscall "NtReplyWaitReplyPort", 0x1091
do_syscall "NtRequestPort", 0x1092
do_syscall "NtRequestWaitReplyPort", 0x1093
do_syscall "NtResetEvent", 0x1094
do_syscall "NtRestoreKey", 0x1095
do_syscall "NtResumeThread", 0x1096
do_syscall "NtSaveKey", 0x1097
do_syscall "NtSetContextThread", 0x1098
do_syscall "NtSetDefaultHardErrorPort", 0x1099
do_syscall "NtSetDefaultLocale", 0x109a
do_syscall "NtSetEaFile", 0x109b
do_syscall "NtSetEvent", 0x109c
do_syscall "NtSetHighEventPair", 0x109d
do_syscall "NtSetHighWaitLowEventPair", 0x109e
do_syscall "NtSetInformationFile", 0x109f
do_syscall "NtSetInformationKey", 0x10a0
do_syscall "NtSetInformationObject", 0x10a1
do_syscall "NtSetInformationProcess", 0x10a2
do_syscall "NtSetInformationThread", 0x10a3
do_syscall "NtSetInformationToken", 0x10a4
do_syscall "NtSetIntervalProfile", 0x10a5
do_syscall "NtSetIoCompletion", 0x10a6
do_syscall "NtSetLdtEntries", 0x10a7
do_syscall "NtSetLowEventPair", 0x10a8
do_syscall "NtSetLowWaitHighEventPair", 0x10a9
do_syscall "NtSetSecurityObject", 0x10aa
do_syscall "NtSetSystemEnvironmentValue", 0x10ab
do_syscall "NtSetSystemInformation", 0x10ac
do_syscall "NtSetSystemPowerState", 0x10ad
do_syscall "NtSetSystemTime", 0x10ae
do_syscall "NtSetTimer", 0x10af
do_syscall "NtSetTimerResolution", 0x10b0
do_syscall "NtSetValueKey", 0x10b1
do_syscall "NtSetVolumeInformationFile", 0x10b2
do_syscall "NtShutdownSystem", 0x10b3
do_syscall "NtSignalAndWaitForSingleObject", 0x10b4
do_syscall "NtStartProfile", 0x10b5
do_syscall "NtStopProfile", 0x10b6
do_syscall "NtSuspendThread", 0x10b7
do_syscall "NtSystemDebugControl", 0x10b8
do_syscall "NtTestAlert", 0x10b9
do_syscall "NtUnloadDriver", 0x10ba
do_syscall "NtUnloadKey", 0x10bb
do_syscall "NtUnlockFile", 0x10bc
do_syscall "NtUnlockVirtualMemory", 0x10bd
do_syscall "NtUnmapViewOfSection", 0x10be
do_syscall "NtVdmControl", 0x10bf
do_syscall "NtWaitForMultipleObjects", 0x10c0
do_syscall "NtWaitForSingleObject", 0x10c1
do_syscall "NtWaitHighEventPair", 0x10c2
do_syscall "NtWaitLowEventPair", 0x10c3
do_syscall "NtWriteFileGather", 0x10c4
do_syscall "NtWriteRequestData", 0x10c5
do_syscall "NtWriteVirtualMemory", 0x10c6
do_syscall "NtYieldExecution", 0x10c7
do_syscall "NtAccessCheckByType", 0x10c8
do_syscall "NtAccessCheckByTypeAndAuditAlarm", 0x10c9
do_syscall "NtAccessCheckByTypeResultList", 0x10ca
do_syscall "NtAccessCheckByTypeResultListAndAuditAlarm", 0x10cb
do_syscall "NtAccessCheckByTypeResultListAndAuditAlarmByHandle", 0x10cc
do_syscall "NtAcquireCMFViewOwnership", 0x10cd
do_syscall "NtAddBootEntry", 0x10ce
do_syscall "NtAddDriverEntry", 0x10cf
do_syscall "NtAllocateUserPhysicalPages", 0x10d0
do_syscall "NtAlpcAcceptConnectPort", 0x10d1
do_syscall "NtAlpcCancelMessage", 0x10d2
do_syscall "NtAlpcConnectPort", 0x10d3
do_syscall "NtAlpcCreatePort", 0x10d4
do_syscall "NtAlpcCreatePortSection", 0x10d5
do_syscall "NtAlpcCreateResourceReserve", 0x10d6
do_syscall "NtAlpcCreateSectionView", 0x10d7
do_syscall "NtAlpcCreateSecurityContext", 0x10d8
do_syscall "NtAlpcDeletePortSection", 0x10d9
do_syscall "NtAlpcDeleteResourceReserve", 0x10da
do_syscall "NtAlpcDeleteSectionView", 0x10db
do_syscall "NtAlpcDeleteSecurityContext", 0x10dc
do_syscall "NtAlpcDisconnectPort", 0x10dd
do_syscall "NtAlpcImpersonateClientOfPort", 0x10de
do_syscall "NtAlpcOpenSenderProcess", 0x10df
do_syscall "NtAlpcOpenSenderThread", 0x10e0
do_syscall "NtAlpcQueryInformation", 0x10e1
do_syscall "NtAlpcQueryInformationMessage", 0x10e2
do_syscall "NtAlpcRevokeSecurityContext", 0x10e3
do_syscall "NtAlpcSendWaitReceivePort", 0x10e4
do_syscall "NtAlpcSetInformation", 0x10e5
do_syscall "NtApphelpCacheControl", 0x10e6
do_syscall "NtAreMappedFilesTheSame", 0x10e7
do_syscall "NtAssignProcessToJobObject", 0x10e8
do_syscall "NtCancelDeviceWakeupRequest", 0x10e9
do_syscall "NtCancelIoFileEx", 0x10ea
do_syscall "NtCancelSynchronousIoFile", 0x10eb
do_syscall "NtClearAllSavepointsTransaction", 0x10ec
do_syscall "NtClearSavepointTransaction", 0x10ed
do_syscall "NtCommitComplete", 0x10ee
do_syscall "NtCommitEnlistment", 0x10ef
do_syscall "NtCommitTransaction", 0x10f0
do_syscall "NtCompactKeys", 0x10f1
do_syscall "NtCompareTokens", 0x10f2
do_syscall "NtCompressKey", 0x10f3
do_syscall "NtCreateDebugObject", 0x10f4
do_syscall "NtCreateEnlistment", 0x10f5
do_syscall "NtCreateJobObject", 0x10f6
do_syscall "NtCreateJobSet", 0x10f7
do_syscall "NtCreateKeyedEvent", 0x10f8
do_syscall "NtCreateKeyTransacted", 0x10f9
do_syscall "NtCreatePrivateNamespace", 0x10fa
do_syscall "NtCreateProcessEx", 0x10fb
do_syscall "NtCreateResourceManager", 0x10fc
do_syscall "NtCreateThreadEx", 0x10fd
do_syscall "NtCreateTransaction", 0x10fe
do_syscall "NtCreateTransactionManager", 0x10ff
do_syscall "NtCreateUserProcess", 0x1100
do_syscall "NtCreateWaitablePort", 0x1101
do_syscall "NtCreateWorkerFactory", 0x1102
do_syscall "NtDebugActiveProcess", 0x1103
do_syscall "NtDebugContinue", 0x1104
do_syscall "NtDeleteBootEntry", 0x1105
do_syscall "NtDeleteDriverEntry", 0x1106
do_syscall "NtDeletePrivateNamespace", 0x1107
do_syscall "NtEnumerateBootEntries", 0x1108
do_syscall "NtEnumerateDriverEntries", 0x1109
do_syscall "NtEnumerateSystemEnvironmentValuesEx", 0x110a
do_syscall "NtEnumerateTransactionObject", 0x110b
do_syscall "NtFilterToken", 0x110c
do_syscall "NtFlushInstallUILanguage", 0x110d
do_syscall "NtFlushProcessWriteBuffers", 0x110e
do_syscall "NtFreeUserPhysicalPages", 0x110f
do_syscall "NtFreezeRegistry", 0x1110
do_syscall "NtFreezeTransactions", 0x1111
do_syscall "NtGetCurrentProcessorNumber", 0x1112
do_syscall "NtGetDevicePowerState", 0x1113
do_syscall "NtGetMUIRegistryInfo", 0x1114
do_syscall "NtGetNextProcess", 0x1115
do_syscall "NtGetNextThread", 0x1116
do_syscall "NtGetNlsSectionPtr", 0x1117
do_syscall "NtGetNotificationResourceManager", 0x1118
do_syscall "NtGetPlugPlayEvent", 0x1119
do_syscall "NtGetWriteWatch", 0x111a
do_syscall "NtImpersonateAnonymousToken", 0x111b
do_syscall "NtInitializeNlsFiles", 0x111c
do_syscall "NtInitiatePowerAction", 0x111d
do_syscall "NtIsProcessInJob", 0x111e
do_syscall "NtIsSystemResumeAutomatic", 0x111f
do_syscall "NtIsUILanguageComitted", 0x1120
do_syscall "NtListTransactions", 0x1121
do_syscall "NtLoadKeyEx", 0x1122
do_syscall "NtLockProductActivationKeys", 0x1123
do_syscall "NtLockRegistryKey", 0x1124
do_syscall "NtMakePermanentObject", 0x1125
do_syscall "NtMapCMFModule", 0x1126
do_syscall "NtMapUserPhysicalPages", 0x1127
do_syscall "NtMapUserPhysicalPagesScatter", 0x1128
do_syscall "NtMarshallTransaction", 0x1129
do_syscall "NtModifyBootEntry", 0x112a
do_syscall "NtModifyDriverEntry", 0x112b
do_syscall "NtNotifyChangeMultipleKeys", 0x112c
do_syscall "NtOpenEnlistment", 0x112d
do_syscall "NtOpenJobObject", 0x112e
do_syscall "NtOpenKeyedEvent", 0x112f
do_syscall "NtOpenKeyTransacted", 0x1130
do_syscall "NtOpenPrivateNamespace", 0x1131
do_syscall "NtOpenProcessTokenEx", 0x1132
do_syscall "NtOpenResourceManager", 0x1133
do_syscall "NtOpenSession", 0x1134
do_syscall "NtOpenThreadTokenEx", 0x1135
do_syscall "NtOpenTransaction", 0x1136
do_syscall "NtOpenTransactionManager", 0x1137
do_syscall "NtPowerInformation", 0x1138
do_syscall "NtPrepareComplete", 0x1139
do_syscall "NtPrepareEnlistment", 0x113a
do_syscall "NtPrePrepareComplete", 0x113b
do_syscall "NtPrePrepareEnlistment", 0x113c
do_syscall "NtPropagationComplete", 0x113d
do_syscall "NtPropagationFailed", 0x113e
do_syscall "NtPullTransaction", 0x113f
do_syscall "NtQueryBootEntryOrder", 0x1140
do_syscall "NtQueryBootOptions", 0x1141
do_syscall "NtQueryDebugFilterState", 0x1142
do_syscall "NtQueryDefaultUILanguage", 0x1143
do_syscall "NtQueryDriverEntryOrder", 0x1144
do_syscall "NtQueryInformationEnlistment", 0x1145
do_syscall "NtQueryInformationJobObject", 0x1146
do_syscall "NtQueryInformationResourceManager", 0x1147
do_syscall "NtQueryInformationTransaction", 0x1148
do_syscall "NtQueryInformationTransactionManager", 0x1149
do_syscall "NtQueryInformationWorkerFactory", 0x114a
do_syscall "NtQueryInstallUILanguage", 0x114b
do_syscall "NtQueryLicenseValue", 0x114c
do_syscall "NtQueryOpenSubKeys", 0x114d
do_syscall "NtQueryOpenSubKeysEx", 0x114e
do_syscall "NtQueryPortInformationProcess", 0x114f
do_syscall "NtQueryQuotaInformationFile", 0x1150
do_syscall "NtQuerySystemEnvironmentValueEx", 0x1151
do_syscall "NtReadOnlyEnlistment", 0x1152
do_syscall "NtRecoverEnlistment", 0x1153
do_syscall "NtRecoverResourceManager", 0x1154
do_syscall "NtRecoverTransactionManager", 0x1155
do_syscall "NtRegisterProtocolAddressInformation", 0x1156
do_syscall "NtReleaseCMFViewOwnership", 0x1157
do_syscall "NtReleaseKeyedEvent", 0x1158
do_syscall "NtReleaseWorkerFactoryWorker", 0x1159
do_syscall "NtRemoveIoCompletionEx", 0x115a
do_syscall "NtRemoveProcessDebug", 0x115b
do_syscall "NtRenameKey", 0x115c
do_syscall "NtRenameTransactionManager", 0x115d
do_syscall "NtReplacePartitionUnit", 0x115e
do_syscall "NtReplyWaitReceivePortEx", 0x115f
do_syscall "NtRequestDeviceWakeup", 0x1160
do_syscall "NtRequestWakeupLatency", 0x1161
do_syscall "NtResetWriteWatch", 0x1162
do_syscall "NtRollbackComplete", 0x1163
do_syscall "NtRollbackEnlistment", 0x1164
do_syscall "NtRollbackSavepointTransaction", 0x1165
do_syscall "NtRollbackTransaction", 0x1166
do_syscall "NtRollforwardTransactionManager", 0x1167
do_syscall "NtSaveKeyEx", 0x1168
do_syscall "NtSaveMergedKeys", 0x1169
do_syscall "NtSavepointComplete", 0x116a
do_syscall "NtSavepointTransaction", 0x116b
do_syscall "NtSecureConnectPort", 0x116c
do_syscall "NtSetBootEntryOrder", 0x116d
do_syscall "NtSetBootOptions", 0x116e
do_syscall "NtSetDebugFilterState", 0x116f
do_syscall "NtSetDefaultUILanguage", 0x1170
do_syscall "NtSetDriverEntryOrder", 0x1171
do_syscall "NtSetEventBoostPriority", 0x1172
do_syscall "NtSetInformationDebugObject", 0x1173
do_syscall "NtSetInformationEnlistment", 0x1174
do_syscall "NtSetInformationJobObject", 0x1175
do_syscall "NtSetInformationResourceManager", 0x1176
do_syscall "NtSetInformationTransaction", 0x1177
do_syscall "NtSetInformationTransactionManager", 0x1178
do_syscall "NtSetInformationWorkerFactory", 0x1179
do_syscall "NtSetQuotaInformationFile", 0x117a
do_syscall "NtSetSystemEnvironmentValueEx", 0x117b
do_syscall "NtSetThreadExecutionState", 0x117c
do_syscall "NtSetUuidSeed", 0x117d
do_syscall "NtShutdownWorkerFactory", 0x117e
do_syscall "NtSinglePhaseReject", 0x117f
do_syscall "NtStartTm", 0x1180
do_syscall "NtSuspendProcess", 0x1181
do_syscall "NtTerminateJobObject", 0x1182
do_syscall "NtThawRegistry", 0x1183
do_syscall "NtThawTransactions", 0x1184
do_syscall "NtTraceControl", 0x1185
do_syscall "NtTraceEvent", 0x1186
do_syscall "NtTranslateFilePath", 0x1187
do_syscall "NtUnloadKey2", 0x1188
do_syscall "NtUnloadKeyEx", 0x1189
do_syscall "NtWaitForDebugEvent", 0x118a
do_syscall "NtWaitForKeyedEvent", 0x118b
do_syscall "NtWaitForMultipleObjects32", 0x118c
do_syscall "NtWaitForWorkViaWorkerFactory", 0x118d
do_syscall "NtWorkerFactoryWorkerReady", 0x118e

.text
.text_end:
