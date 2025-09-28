.intel_syntax noprefix

#include "context_offsets.h"
#include "context.S"
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

dwarf_func_asm init_syscalls
dwarf_func_asm syscall_handler

.uleb128 0

.debug_info_end:

.macro syscall_stub name
    .text
    .global "stub_\name"
    "stub_\name":
        lea rcx, [rip+0f]
        jmp unimplemented_syscall
    0:
        .ascii "\name\0"
.endm

syscall_stub "NtAcceptConnectPort"
syscall_stub "NtAccessCheck"
syscall_stub "NtAccessCheckAndAuditAlarm"
syscall_stub "NtAddAtom"
syscall_stub "NtAdjustGroupsToken"
syscall_stub "NtAdjustPrivilegesToken"
syscall_stub "NtAlertResumeThread"
syscall_stub "NtAlertThread"
syscall_stub "NtAllocateLocallyUniqueId"
syscall_stub "NtAllocateUuids"
syscall_stub "NtCallbackReturn"
syscall_stub "NtCancelIoFile"
syscall_stub "NtClearEvent"
syscall_stub "NtCloseObjectAuditAlarm"
syscall_stub "NtCompleteConnectPort"
syscall_stub "NtConnectPort"
syscall_stub "NtCreateDirectoryObject"
syscall_stub "NtCreateEventPair"
syscall_stub "NtCreateIoCompletion"
syscall_stub "NtCreateKey"
syscall_stub "NtCreateMailslotFile"
syscall_stub "NtCreateMutant"
syscall_stub "NtCreatePagingFile"
syscall_stub "NtCreatePort"
syscall_stub "NtCreateProfile"
syscall_stub "NtCreateSemaphore"
syscall_stub "NtCreateSymbolicLinkObject"
syscall_stub "NtCreateToken"
syscall_stub "NtDeleteAtom"
syscall_stub "NtDeleteFile"
syscall_stub "NtDeleteKey"
syscall_stub "NtDeleteObjectAuditAlarm"
syscall_stub "NtDeleteValueKey"
syscall_stub "NtDuplicateToken"
syscall_stub "NtEnumerateKey"
syscall_stub "NtEnumerateValueKey"
syscall_stub "NtExtendSection"
syscall_stub "NtFindAtom"
syscall_stub "NtFlushBuffersFile"
syscall_stub "NtFlushInstructionCache"
syscall_stub "NtFlushKey"
syscall_stub "NtFlushWriteBuffer"
syscall_stub "NtFsControlFile"
syscall_stub "NtImpersonateClientOfPort"
syscall_stub "NtImpersonateThread"
syscall_stub "NtInitializeRegistry"
syscall_stub "NtListenPort"
syscall_stub "NtLoadDriver"
syscall_stub "NtLoadKey"
syscall_stub "NtLoadKey2"
syscall_stub "NtLockFile"
syscall_stub "NtLockVirtualMemory"
syscall_stub "NtMakeTemporaryObject"
syscall_stub "NtNotifyChangeDirectoryFile"
syscall_stub "NtNotifyChangeKey"
syscall_stub "NtOpenDirectoryObject"
syscall_stub "NtOpenEvent"
syscall_stub "NtOpenEventPair"
syscall_stub "NtOpenIoCompletion"
syscall_stub "NtOpenKey"
syscall_stub "NtOpenMutant"
syscall_stub "NtOpenObjectAuditAlarm"
syscall_stub "NtOpenProcessToken"
syscall_stub "NtOpenSemaphore"
syscall_stub "NtOpenSymbolicLinkObject"
syscall_stub "NtOpenThread"
syscall_stub "NtOpenThreadToken"
syscall_stub "NtOpenTimer"
syscall_stub "NtPlugPlayControl"
syscall_stub "NtPrivilegeCheck"
syscall_stub "NtPrivilegeObjectAuditAlarm"
syscall_stub "NtPrivilegedServiceAuditAlarm"
syscall_stub "NtPulseEvent"
syscall_stub "NtQueryAttributesFile"
syscall_stub "NtQueryDefaultLocale"
syscall_stub "NtQueryDirectoryObject"
syscall_stub "NtQueryEaFile"
syscall_stub "NtQueryEvent"
syscall_stub "NtQueryFullAttributesFile"
syscall_stub "NtQueryInformationAtom"
syscall_stub "NtQueryInformationPort"
syscall_stub "NtQueryInformationToken"
syscall_stub "NtQueryIntervalProfile"
syscall_stub "NtQueryIoCompletion"
syscall_stub "NtQueryKey"
syscall_stub "NtQueryMultipleValueKey"
syscall_stub "NtQueryMutant"
syscall_stub "NtQueryObject"
syscall_stub "NtQueryPerformanceCounter"
syscall_stub "NtQuerySecurityObject"
syscall_stub "NtQuerySemaphore"
syscall_stub "NtQuerySymbolicLinkObject"
syscall_stub "NtQuerySystemEnvironmentValue"
syscall_stub "NtQuerySystemInformation"
syscall_stub "NtQueryTimer"
syscall_stub "NtQueryTimerResolution"
syscall_stub "NtQueryValueKey"
syscall_stub "NtQueryVirtualMemory"
syscall_stub "NtQueryVolumeInformationFile"
syscall_stub "NtQueueApcThread"
syscall_stub "NtRaiseException"
syscall_stub "NtRaiseHardError"
syscall_stub "NtReadFileScatter"
syscall_stub "NtReadRequestData"
syscall_stub "NtRegisterThreadTerminatePort"
syscall_stub "NtReleaseMutant"
syscall_stub "NtReleaseSemaphore"
syscall_stub "NtRemoveIoCompletion"
syscall_stub "NtReplaceKey"
syscall_stub "NtReplyPort"
syscall_stub "NtReplyWaitReceivePort"
syscall_stub "NtReplyWaitReplyPort"
syscall_stub "NtRequestPort"
syscall_stub "NtRequestWaitReplyPort"
syscall_stub "NtResetEvent"
syscall_stub "NtRestoreKey"
syscall_stub "NtSaveKey"
syscall_stub "NtSetDefaultHardErrorPort"
syscall_stub "NtSetDefaultLocale"
syscall_stub "NtSetEaFile"
syscall_stub "NtSetHighEventPair"
syscall_stub "NtSetHighWaitLowEventPair"
syscall_stub "NtSetInformationKey"
syscall_stub "NtSetInformationObject"
syscall_stub "NtSetInformationProcess"
syscall_stub "NtSetInformationThread"
syscall_stub "NtSetInformationToken"
syscall_stub "NtSetIntervalProfile"
syscall_stub "NtSetIoCompletion"
syscall_stub "NtSetLdtEntries"
syscall_stub "NtSetLowEventPair"
syscall_stub "NtSetLowWaitHighEventPair"
syscall_stub "NtSetSecurityObject"
syscall_stub "NtSetSystemEnvironmentValue"
syscall_stub "NtSetSystemInformation"
syscall_stub "NtSetSystemPowerState"
syscall_stub "NtSetSystemTime"
syscall_stub "NtSetTimerResolution"
syscall_stub "NtSetValueKey"
syscall_stub "NtSetVolumeInformationFile"
syscall_stub "NtSignalAndWaitForSingleObject"
syscall_stub "NtStartProfile"
syscall_stub "NtStopProfile"
syscall_stub "NtSystemDebugControl"
syscall_stub "NtTestAlert"
syscall_stub "NtUnloadDriver"
syscall_stub "NtUnloadKey"
syscall_stub "NtUnlockFile"
syscall_stub "NtUnlockVirtualMemory"
syscall_stub "NtVdmControl"
syscall_stub "NtWaitHighEventPair"
syscall_stub "NtWaitLowEventPair"
syscall_stub "NtWriteFileGather"
syscall_stub "NtWriteRequestData"
syscall_stub "NtYieldExecution"
syscall_stub "NtAccessCheckByType"
syscall_stub "NtAccessCheckByTypeAndAuditAlarm"
syscall_stub "NtAccessCheckByTypeResultList"
syscall_stub "NtAccessCheckByTypeResultListAndAuditAlarm"
syscall_stub "NtAccessCheckByTypeResultListAndAuditAlarmByHandle"
syscall_stub "NtAcquireCMFViewOwnership"
syscall_stub "NtAddBootEntry"
syscall_stub "NtAddDriverEntry"
syscall_stub "NtAllocateUserPhysicalPages"
syscall_stub "NtAlpcAcceptConnectPort"
syscall_stub "NtAlpcCancelMessage"
syscall_stub "NtAlpcConnectPort"
syscall_stub "NtAlpcCreatePort"
syscall_stub "NtAlpcCreatePortSection"
syscall_stub "NtAlpcCreateResourceReserve"
syscall_stub "NtAlpcCreateSectionView"
syscall_stub "NtAlpcCreateSecurityContext"
syscall_stub "NtAlpcDeletePortSection"
syscall_stub "NtAlpcDeleteResourceReserve"
syscall_stub "NtAlpcDeleteSectionView"
syscall_stub "NtAlpcDeleteSecurityContext"
syscall_stub "NtAlpcDisconnectPort"
syscall_stub "NtAlpcImpersonateClientOfPort"
syscall_stub "NtAlpcOpenSenderProcess"
syscall_stub "NtAlpcOpenSenderThread"
syscall_stub "NtAlpcQueryInformation"
syscall_stub "NtAlpcQueryInformationMessage"
syscall_stub "NtAlpcRevokeSecurityContext"
syscall_stub "NtAlpcSendWaitReceivePort"
syscall_stub "NtAlpcSetInformation"
syscall_stub "NtApphelpCacheControl"
syscall_stub "NtAreMappedFilesTheSame"
syscall_stub "NtAssignProcessToJobObject"
syscall_stub "NtCancelDeviceWakeupRequest"
syscall_stub "NtCancelIoFileEx"
syscall_stub "NtCancelSynchronousIoFile"
syscall_stub "NtClearAllSavepointsTransaction"
syscall_stub "NtClearSavepointTransaction"
syscall_stub "NtCommitComplete"
syscall_stub "NtCommitEnlistment"
syscall_stub "NtCommitTransaction"
syscall_stub "NtCompactKeys"
syscall_stub "NtCompareTokens"
syscall_stub "NtCompressKey"
syscall_stub "NtCreateDebugObject"
syscall_stub "NtCreateEnlistment"
syscall_stub "NtCreateJobObject"
syscall_stub "NtCreateJobSet"
syscall_stub "NtCreateKeyedEvent"
syscall_stub "NtCreateKeyTransacted"
syscall_stub "NtCreatePrivateNamespace"
syscall_stub "NtCreateProcessEx"
syscall_stub "NtCreateResourceManager"
syscall_stub "NtCreateThreadEx"
syscall_stub "NtCreateTransaction"
syscall_stub "NtCreateTransactionManager"
syscall_stub "NtCreateUserProcess"
syscall_stub "NtCreateWaitablePort"
syscall_stub "NtCreateWorkerFactory"
syscall_stub "NtDebugActiveProcess"
syscall_stub "NtDebugContinue"
syscall_stub "NtDeleteBootEntry"
syscall_stub "NtDeleteDriverEntry"
syscall_stub "NtDeletePrivateNamespace"
syscall_stub "NtEnumerateBootEntries"
syscall_stub "NtEnumerateDriverEntries"
syscall_stub "NtEnumerateSystemEnvironmentValuesEx"
syscall_stub "NtEnumerateTransactionObject"
syscall_stub "NtFilterToken"
syscall_stub "NtFlushInstallUILanguage"
syscall_stub "NtFlushProcessWriteBuffers"
syscall_stub "NtFreeUserPhysicalPages"
syscall_stub "NtFreezeRegistry"
syscall_stub "NtFreezeTransactions"
syscall_stub "NtGetCurrentProcessorNumber"
syscall_stub "NtGetDevicePowerState"
syscall_stub "NtGetMUIRegistryInfo"
syscall_stub "NtGetNlsSectionPtr"
syscall_stub "NtGetNotificationResourceManager"
syscall_stub "NtGetPlugPlayEvent"
syscall_stub "NtGetWriteWatch"
syscall_stub "NtImpersonateAnonymousToken"
syscall_stub "NtInitializeNlsFiles"
syscall_stub "NtInitiatePowerAction"
syscall_stub "NtIsProcessInJob"
syscall_stub "NtIsSystemResumeAutomatic"
syscall_stub "NtIsUILanguageComitted"
syscall_stub "NtListTransactions"
syscall_stub "NtLoadKeyEx"
syscall_stub "NtLockProductActivationKeys"
syscall_stub "NtLockRegistryKey"
syscall_stub "NtMakePermanentObject"
syscall_stub "NtMapCMFModule"
syscall_stub "NtMapUserPhysicalPages"
syscall_stub "NtMapUserPhysicalPagesScatter"
syscall_stub "NtMarshallTransaction"
syscall_stub "NtModifyBootEntry"
syscall_stub "NtModifyDriverEntry"
syscall_stub "NtNotifyChangeMultipleKeys"
syscall_stub "NtOpenEnlistment"
syscall_stub "NtOpenJobObject"
syscall_stub "NtOpenKeyedEvent"
syscall_stub "NtOpenKeyTransacted"
syscall_stub "NtOpenPrivateNamespace"
syscall_stub "NtOpenProcessTokenEx"
syscall_stub "NtOpenResourceManager"
syscall_stub "NtOpenSession"
syscall_stub "NtOpenThreadTokenEx"
syscall_stub "NtOpenTransaction"
syscall_stub "NtOpenTransactionManager"
syscall_stub "NtPowerInformation"
syscall_stub "NtPrepareComplete"
syscall_stub "NtPrepareEnlistment"
syscall_stub "NtPrePrepareComplete"
syscall_stub "NtPrePrepareEnlistment"
syscall_stub "NtPropagationComplete"
syscall_stub "NtPropagationFailed"
syscall_stub "NtPullTransaction"
syscall_stub "NtQueryBootEntryOrder"
syscall_stub "NtQueryBootOptions"
syscall_stub "NtQueryDebugFilterState"
syscall_stub "NtQueryDefaultUILanguage"
syscall_stub "NtQueryDriverEntryOrder"
syscall_stub "NtQueryInformationEnlistment"
syscall_stub "NtQueryInformationJobObject"
syscall_stub "NtQueryInformationResourceManager"
syscall_stub "NtQueryInformationTransaction"
syscall_stub "NtQueryInformationTransactionManager"
syscall_stub "NtQueryInformationWorkerFactory"
syscall_stub "NtQueryInstallUILanguage"
syscall_stub "NtQueryLicenseValue"
syscall_stub "NtQueryOpenSubKeys"
syscall_stub "NtQueryOpenSubKeysEx"
syscall_stub "NtQueryPortInformationProcess"
syscall_stub "NtQueryQuotaInformationFile"
syscall_stub "NtQuerySystemEnvironmentValueEx"
syscall_stub "NtReadOnlyEnlistment"
syscall_stub "NtRecoverEnlistment"
syscall_stub "NtRecoverResourceManager"
syscall_stub "NtRecoverTransactionManager"
syscall_stub "NtRegisterProtocolAddressInformation"
syscall_stub "NtReleaseCMFViewOwnership"
syscall_stub "NtReleaseKeyedEvent"
syscall_stub "NtReleaseWorkerFactoryWorker"
syscall_stub "NtRemoveIoCompletionEx"
syscall_stub "NtRemoveProcessDebug"
syscall_stub "NtRenameKey"
syscall_stub "NtRenameTransactionManager"
syscall_stub "NtReplacePartitionUnit"
syscall_stub "NtReplyWaitReceivePortEx"
syscall_stub "NtRequestDeviceWakeup"
syscall_stub "NtRequestWakeupLatency"
syscall_stub "NtResetWriteWatch"
syscall_stub "NtRollbackComplete"
syscall_stub "NtRollbackEnlistment"
syscall_stub "NtRollbackSavepointTransaction"
syscall_stub "NtRollbackTransaction"
syscall_stub "NtRollforwardTransactionManager"
syscall_stub "NtSaveKeyEx"
syscall_stub "NtSaveMergedKeys"
syscall_stub "NtSavepointComplete"
syscall_stub "NtSavepointTransaction"
syscall_stub "NtSecureConnectPort"
syscall_stub "NtSetBootEntryOrder"
syscall_stub "NtSetBootOptions"
syscall_stub "NtSetDebugFilterState"
syscall_stub "NtSetDefaultUILanguage"
syscall_stub "NtSetDriverEntryOrder"
syscall_stub "NtSetEventBoostPriority"
syscall_stub "NtSetInformationDebugObject"
syscall_stub "NtSetInformationEnlistment"
syscall_stub "NtSetInformationJobObject"
syscall_stub "NtSetInformationResourceManager"
syscall_stub "NtSetInformationTransaction"
syscall_stub "NtSetInformationTransactionManager"
syscall_stub "NtSetInformationWorkerFactory"
syscall_stub "NtSetQuotaInformationFile"
syscall_stub "NtSetSystemEnvironmentValueEx"
syscall_stub "NtSetThreadExecutionState"
syscall_stub "NtSetUuidSeed"
syscall_stub "NtShutdownWorkerFactory"
syscall_stub "NtSinglePhaseReject"
syscall_stub "NtStartTm"
syscall_stub "NtTerminateJobObject"
syscall_stub "NtThawRegistry"
syscall_stub "NtThawTransactions"
syscall_stub "NtTraceControl"
syscall_stub "NtTraceEvent"
syscall_stub "NtTranslateFilePath"
syscall_stub "NtUnloadKey2"
syscall_stub "NtUnloadKeyEx"
syscall_stub "NtWaitForDebugEvent"
syscall_stub "NtWaitForKeyedEvent"
syscall_stub "NtWaitForMultipleObjects32"
syscall_stub "NtWaitForWorkViaWorkerFactory"
syscall_stub "NtWorkerFactoryWorkerReady"

.section .rdata
syscalls:
    .quad user_NtDisplayString
    .quad user_NtTerminateThread
    .quad user_NtTerminateProcess
    .quad user_NtWriteFile
    .quad user_NtResumeProcess
    .quad user_NtAllocateVirtualMemory
    .quad user_NtFreeVirtualMemory
    .quad user_NtReadFile
    .quad user_NtProtectVirtualMemory
    .quad stub_NtAcceptConnectPort
    .quad stub_NtAccessCheck
    .quad stub_NtAccessCheckAndAuditAlarm
    .quad stub_NtAddAtom
    .quad stub_NtAdjustGroupsToken
    .quad stub_NtAdjustPrivilegesToken
    .quad stub_NtAlertResumeThread
    .quad stub_NtAlertThread
    .quad stub_NtAllocateLocallyUniqueId
    .quad stub_NtAllocateUuids
    .quad stub_NtCallbackReturn
    .quad stub_NtCancelIoFile
    .quad user_NtCancelTimer
    .quad stub_NtClearEvent
    .quad user_NtClose
    .quad stub_NtCloseObjectAuditAlarm
    .quad stub_NtCompleteConnectPort
    .quad stub_NtConnectPort
    .quad stub_NtCreateDirectoryObject
    .quad user_NtCreateEvent
    .quad stub_NtCreateEventPair
    .quad user_NtCreateFile
    .quad stub_NtCreateIoCompletion
    .quad stub_NtCreateKey
    .quad stub_NtCreateMailslotFile
    .quad stub_NtCreateMutant
    .quad user_NtCreateNamedPipeFile
    .quad stub_NtCreatePagingFile
    .quad stub_NtCreatePort
    .quad user_NtCreateProcess
    .quad stub_NtCreateProfile
    .quad user_NtCreateSection
    .quad stub_NtCreateSemaphore
    .quad stub_NtCreateSymbolicLinkObject
    .quad user_NtCreateThread
    .quad user_NtCreateTimer
    .quad stub_NtCreateToken
    .quad user_NtDelayExecution
    .quad stub_NtDeleteAtom
    .quad stub_NtDeleteFile
    .quad stub_NtDeleteKey
    .quad stub_NtDeleteObjectAuditAlarm
    .quad stub_NtDeleteValueKey
    .quad user_NtDeviceIoControlFile
    .quad user_NtDuplicateObject
    .quad stub_NtDuplicateToken
    .quad stub_NtEnumerateKey
    .quad stub_NtEnumerateValueKey
    .quad stub_NtExtendSection
    .quad stub_NtFindAtom
    .quad stub_NtFlushBuffersFile
    .quad stub_NtFlushInstructionCache
    .quad stub_NtFlushKey
    .quad user_NtFlushVirtualMemory
    .quad stub_NtFlushWriteBuffer
    .quad stub_NtFsControlFile
    .quad user_NtGetContextThread
    .quad stub_NtImpersonateClientOfPort
    .quad stub_NtImpersonateThread
    .quad stub_NtInitializeRegistry
    .quad stub_NtListenPort
    .quad stub_NtLoadDriver
    .quad stub_NtLoadKey
    .quad stub_NtLoadKey2
    .quad stub_NtLockFile
    .quad stub_NtLockVirtualMemory
    .quad stub_NtMakeTemporaryObject
    .quad user_NtMapViewOfSection
    .quad stub_NtNotifyChangeDirectoryFile
    .quad stub_NtNotifyChangeKey
    .quad stub_NtOpenDirectoryObject
    .quad stub_NtOpenEvent
    .quad stub_NtOpenEventPair
    .quad user_NtOpenFile
    .quad stub_NtOpenIoCompletion
    .quad stub_NtOpenKey
    .quad stub_NtOpenMutant
    .quad stub_NtOpenObjectAuditAlarm
    .quad user_NtOpenProcess
    .quad stub_NtOpenProcessToken
    .quad user_NtOpenSection
    .quad stub_NtOpenSemaphore
    .quad stub_NtOpenSymbolicLinkObject
    .quad stub_NtOpenThread
    .quad stub_NtOpenThreadToken
    .quad stub_NtOpenTimer
    .quad stub_NtPlugPlayControl
    .quad stub_NtPrivilegeCheck
    .quad stub_NtPrivilegeObjectAuditAlarm
    .quad stub_NtPrivilegedServiceAuditAlarm
    .quad stub_NtPulseEvent
    .quad stub_NtQueryAttributesFile
    .quad stub_NtQueryDefaultLocale
    .quad user_NtQueryDirectoryFile
    .quad stub_NtQueryDirectoryObject
    .quad stub_NtQueryEaFile
    .quad stub_NtQueryEvent
    .quad stub_NtQueryFullAttributesFile
    .quad stub_NtQueryInformationAtom
    .quad user_NtQueryInformationFile
    .quad stub_NtQueryInformationPort
    .quad user_NtQueryInformationProcess
    .quad user_NtQueryInformationThread
    .quad stub_NtQueryInformationToken
    .quad stub_NtQueryIntervalProfile
    .quad stub_NtQueryIoCompletion
    .quad stub_NtQueryKey
    .quad stub_NtQueryMultipleValueKey
    .quad stub_NtQueryMutant
    .quad stub_NtQueryObject
    .quad stub_NtQueryPerformanceCounter
    .quad user_NtQuerySection
    .quad stub_NtQuerySecurityObject
    .quad stub_NtQuerySemaphore
    .quad stub_NtQuerySymbolicLinkObject
    .quad stub_NtQuerySystemEnvironmentValue
    .quad stub_NtQuerySystemInformation
    .quad user_NtQuerySystemTime
    .quad stub_NtQueryTimer
    .quad stub_NtQueryTimerResolution
    .quad stub_NtQueryValueKey
    .quad stub_NtQueryVirtualMemory
    .quad stub_NtQueryVolumeInformationFile
    .quad stub_NtQueueApcThread
    .quad stub_NtRaiseException
    .quad stub_NtRaiseHardError
    .quad stub_NtReadFileScatter
    .quad stub_NtReadRequestData
    .quad user_NtReadVirtualMemory
    .quad stub_NtRegisterThreadTerminatePort
    .quad stub_NtReleaseMutant
    .quad stub_NtReleaseSemaphore
    .quad stub_NtRemoveIoCompletion
    .quad stub_NtReplaceKey
    .quad stub_NtReplyPort
    .quad stub_NtReplyWaitReceivePort
    .quad stub_NtReplyWaitReplyPort
    .quad stub_NtRequestPort
    .quad stub_NtRequestWaitReplyPort
    .quad stub_NtResetEvent
    .quad stub_NtRestoreKey
    .quad user_NtResumeThread
    .quad stub_NtSaveKey
    .quad user_NtSetContextThread
    .quad stub_NtSetDefaultHardErrorPort
    .quad stub_NtSetDefaultLocale
    .quad stub_NtSetEaFile
    .quad user_NtSetEvent
    .quad stub_NtSetHighEventPair
    .quad stub_NtSetHighWaitLowEventPair
    .quad user_NtSetInformationFile
    .quad stub_NtSetInformationKey
    .quad stub_NtSetInformationObject
    .quad stub_NtSetInformationProcess
    .quad stub_NtSetInformationThread
    .quad stub_NtSetInformationToken
    .quad stub_NtSetIntervalProfile
    .quad stub_NtSetIoCompletion
    .quad stub_NtSetLdtEntries
    .quad stub_NtSetLowEventPair
    .quad stub_NtSetLowWaitHighEventPair
    .quad stub_NtSetSecurityObject
    .quad stub_NtSetSystemEnvironmentValue
    .quad stub_NtSetSystemInformation
    .quad stub_NtSetSystemPowerState
    .quad stub_NtSetSystemTime
    .quad user_NtSetTimer
    .quad stub_NtSetTimerResolution
    .quad stub_NtSetValueKey
    .quad stub_NtSetVolumeInformationFile
    .quad user_NtShutdownSystem
    .quad stub_NtSignalAndWaitForSingleObject
    .quad stub_NtStartProfile
    .quad stub_NtStopProfile
    .quad user_NtSuspendThread
    .quad stub_NtSystemDebugControl
    .quad stub_NtTestAlert
    .quad stub_NtUnloadDriver
    .quad stub_NtUnloadKey
    .quad stub_NtUnlockFile
    .quad stub_NtUnlockVirtualMemory
    .quad user_NtUnmapViewOfSection
    .quad stub_NtVdmControl
    .quad user_NtWaitForMultipleObjects
    .quad user_NtWaitForSingleObject
    .quad stub_NtWaitHighEventPair
    .quad stub_NtWaitLowEventPair
    .quad stub_NtWriteFileGather
    .quad stub_NtWriteRequestData
    .quad user_NtWriteVirtualMemory
    .quad stub_NtYieldExecution
    .quad stub_NtAccessCheckByType
    .quad stub_NtAccessCheckByTypeAndAuditAlarm
    .quad stub_NtAccessCheckByTypeResultList
    .quad stub_NtAccessCheckByTypeResultListAndAuditAlarm
    .quad stub_NtAccessCheckByTypeResultListAndAuditAlarmByHandle
    .quad stub_NtAcquireCMFViewOwnership
    .quad stub_NtAddBootEntry
    .quad stub_NtAddDriverEntry
    .quad stub_NtAllocateUserPhysicalPages
    .quad stub_NtAlpcAcceptConnectPort
    .quad stub_NtAlpcCancelMessage
    .quad stub_NtAlpcConnectPort
    .quad stub_NtAlpcCreatePort
    .quad stub_NtAlpcCreatePortSection
    .quad stub_NtAlpcCreateResourceReserve
    .quad stub_NtAlpcCreateSectionView
    .quad stub_NtAlpcCreateSecurityContext
    .quad stub_NtAlpcDeletePortSection
    .quad stub_NtAlpcDeleteResourceReserve
    .quad stub_NtAlpcDeleteSectionView
    .quad stub_NtAlpcDeleteSecurityContext
    .quad stub_NtAlpcDisconnectPort
    .quad stub_NtAlpcImpersonateClientOfPort
    .quad stub_NtAlpcOpenSenderProcess
    .quad stub_NtAlpcOpenSenderThread
    .quad stub_NtAlpcQueryInformation
    .quad stub_NtAlpcQueryInformationMessage
    .quad stub_NtAlpcRevokeSecurityContext
    .quad stub_NtAlpcSendWaitReceivePort
    .quad stub_NtAlpcSetInformation
    .quad stub_NtApphelpCacheControl
    .quad stub_NtAreMappedFilesTheSame
    .quad stub_NtAssignProcessToJobObject
    .quad stub_NtCancelDeviceWakeupRequest
    .quad stub_NtCancelIoFileEx
    .quad stub_NtCancelSynchronousIoFile
    .quad stub_NtClearAllSavepointsTransaction
    .quad stub_NtClearSavepointTransaction
    .quad stub_NtCommitComplete
    .quad stub_NtCommitEnlistment
    .quad stub_NtCommitTransaction
    .quad stub_NtCompactKeys
    .quad stub_NtCompareTokens
    .quad stub_NtCompressKey
    .quad stub_NtCreateDebugObject
    .quad stub_NtCreateEnlistment
    .quad stub_NtCreateJobObject
    .quad stub_NtCreateJobSet
    .quad stub_NtCreateKeyedEvent
    .quad stub_NtCreateKeyTransacted
    .quad stub_NtCreatePrivateNamespace
    .quad stub_NtCreateProcessEx
    .quad stub_NtCreateResourceManager
    .quad stub_NtCreateThreadEx
    .quad stub_NtCreateTransaction
    .quad stub_NtCreateTransactionManager
    .quad stub_NtCreateUserProcess
    .quad stub_NtCreateWaitablePort
    .quad stub_NtCreateWorkerFactory
    .quad stub_NtDebugActiveProcess
    .quad stub_NtDebugContinue
    .quad stub_NtDeleteBootEntry
    .quad stub_NtDeleteDriverEntry
    .quad stub_NtDeletePrivateNamespace
    .quad stub_NtEnumerateBootEntries
    .quad stub_NtEnumerateDriverEntries
    .quad stub_NtEnumerateSystemEnvironmentValuesEx
    .quad stub_NtEnumerateTransactionObject
    .quad stub_NtFilterToken
    .quad stub_NtFlushInstallUILanguage
    .quad stub_NtFlushProcessWriteBuffers
    .quad stub_NtFreeUserPhysicalPages
    .quad stub_NtFreezeRegistry
    .quad stub_NtFreezeTransactions
    .quad stub_NtGetCurrentProcessorNumber
    .quad stub_NtGetDevicePowerState
    .quad stub_NtGetMUIRegistryInfo
    .quad user_NtGetNextProcess
    .quad user_NtGetNextThread
    .quad stub_NtGetNlsSectionPtr
    .quad stub_NtGetNotificationResourceManager
    .quad stub_NtGetPlugPlayEvent
    .quad stub_NtGetWriteWatch
    .quad stub_NtImpersonateAnonymousToken
    .quad stub_NtInitializeNlsFiles
    .quad stub_NtInitiatePowerAction
    .quad stub_NtIsProcessInJob
    .quad stub_NtIsSystemResumeAutomatic
    .quad stub_NtIsUILanguageComitted
    .quad stub_NtListTransactions
    .quad stub_NtLoadKeyEx
    .quad stub_NtLockProductActivationKeys
    .quad stub_NtLockRegistryKey
    .quad stub_NtMakePermanentObject
    .quad stub_NtMapCMFModule
    .quad stub_NtMapUserPhysicalPages
    .quad stub_NtMapUserPhysicalPagesScatter
    .quad stub_NtMarshallTransaction
    .quad stub_NtModifyBootEntry
    .quad stub_NtModifyDriverEntry
    .quad stub_NtNotifyChangeMultipleKeys
    .quad stub_NtOpenEnlistment
    .quad stub_NtOpenJobObject
    .quad stub_NtOpenKeyedEvent
    .quad stub_NtOpenKeyTransacted
    .quad stub_NtOpenPrivateNamespace
    .quad stub_NtOpenProcessTokenEx
    .quad stub_NtOpenResourceManager
    .quad stub_NtOpenSession
    .quad stub_NtOpenThreadTokenEx
    .quad stub_NtOpenTransaction
    .quad stub_NtOpenTransactionManager
    .quad stub_NtPowerInformation
    .quad stub_NtPrepareComplete
    .quad stub_NtPrepareEnlistment
    .quad stub_NtPrePrepareComplete
    .quad stub_NtPrePrepareEnlistment
    .quad stub_NtPropagationComplete
    .quad stub_NtPropagationFailed
    .quad stub_NtPullTransaction
    .quad stub_NtQueryBootEntryOrder
    .quad stub_NtQueryBootOptions
    .quad stub_NtQueryDebugFilterState
    .quad stub_NtQueryDefaultUILanguage
    .quad stub_NtQueryDriverEntryOrder
    .quad stub_NtQueryInformationEnlistment
    .quad stub_NtQueryInformationJobObject
    .quad stub_NtQueryInformationResourceManager
    .quad stub_NtQueryInformationTransaction
    .quad stub_NtQueryInformationTransactionManager
    .quad stub_NtQueryInformationWorkerFactory
    .quad stub_NtQueryInstallUILanguage
    .quad stub_NtQueryLicenseValue
    .quad stub_NtQueryOpenSubKeys
    .quad stub_NtQueryOpenSubKeysEx
    .quad stub_NtQueryPortInformationProcess
    .quad stub_NtQueryQuotaInformationFile
    .quad stub_NtQuerySystemEnvironmentValueEx
    .quad stub_NtReadOnlyEnlistment
    .quad stub_NtRecoverEnlistment
    .quad stub_NtRecoverResourceManager
    .quad stub_NtRecoverTransactionManager
    .quad stub_NtRegisterProtocolAddressInformation
    .quad stub_NtReleaseCMFViewOwnership
    .quad stub_NtReleaseKeyedEvent
    .quad stub_NtReleaseWorkerFactoryWorker
    .quad stub_NtRemoveIoCompletionEx
    .quad stub_NtRemoveProcessDebug
    .quad stub_NtRenameKey
    .quad stub_NtRenameTransactionManager
    .quad stub_NtReplacePartitionUnit
    .quad stub_NtReplyWaitReceivePortEx
    .quad stub_NtRequestDeviceWakeup
    .quad stub_NtRequestWakeupLatency
    .quad stub_NtResetWriteWatch
    .quad stub_NtRollbackComplete
    .quad stub_NtRollbackEnlistment
    .quad stub_NtRollbackSavepointTransaction
    .quad stub_NtRollbackTransaction
    .quad stub_NtRollforwardTransactionManager
    .quad stub_NtSaveKeyEx
    .quad stub_NtSaveMergedKeys
    .quad stub_NtSavepointComplete
    .quad stub_NtSavepointTransaction
    .quad stub_NtSecureConnectPort
    .quad stub_NtSetBootEntryOrder
    .quad stub_NtSetBootOptions
    .quad stub_NtSetDebugFilterState
    .quad stub_NtSetDefaultUILanguage
    .quad stub_NtSetDriverEntryOrder
    .quad stub_NtSetEventBoostPriority
    .quad stub_NtSetInformationDebugObject
    .quad stub_NtSetInformationEnlistment
    .quad stub_NtSetInformationJobObject
    .quad stub_NtSetInformationResourceManager
    .quad stub_NtSetInformationTransaction
    .quad stub_NtSetInformationTransactionManager
    .quad stub_NtSetInformationWorkerFactory
    .quad stub_NtSetQuotaInformationFile
    .quad stub_NtSetSystemEnvironmentValueEx
    .quad stub_NtSetThreadExecutionState
    .quad stub_NtSetUuidSeed
    .quad stub_NtShutdownWorkerFactory
    .quad stub_NtSinglePhaseReject
    .quad stub_NtStartTm
    .quad user_NtSuspendProcess
    .quad stub_NtTerminateJobObject
    .quad stub_NtThawRegistry
    .quad stub_NtThawTransactions
    .quad stub_NtTraceControl
    .quad stub_NtTraceEvent
    .quad stub_NtTranslateFilePath
    .quad stub_NtUnloadKey2
    .quad stub_NtUnloadKeyEx
    .quad stub_NtWaitForDebugEvent
    .quad stub_NtWaitForKeyedEvent
    .quad stub_NtWaitForMultipleObjects32
    .quad stub_NtWaitForWorkViaWorkerFactory
    .quad stub_NtWorkerFactoryWorkerReady
end_syscalls:

# FIXME - Clang forces the nice version to be a memory address (GAS is okay)
# .equ NUM_SYSCALLS, (end_syscalls - syscalls) / 8
.equ NUM_SYSCALLS, 0x18f

.section .text

.global init_syscalls
init_syscalls:
    mov ecx, 0xc0000082 # LSTAR
    lea rax, [rip+syscall_handler]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    mov ecx, 0xc0000081 # STAR
    xor eax, eax
    mov edx, 0x002b0008 # ((user CS - 16) << 16) | kernel CS
    wrmsr

    mov ecx, 0xc0000084 # SFMASK
    xor edx, edx
    mov eax, 0x200 # disable interrupts when syscall_handler called
    wrmsr

    ret
init_syscalls_end:

.global syscall_handler
syscall_handler:
    swapgs
    save_syscall_context

    # change to kernel stack
    mov rax, gs:GS_CURRENT_THREAD
    mov rsp, [rax + THREAD_SYSCALL_STACK]

    # mark thread as running kernel code
    mov BYTE PTR [rax + THREAD_KERNEL_MODE], 1

    # load kernel cr3
    mov rax, gs:GS_KERNEL_CR3
    mov cr3, rax

    mov rax, gs:GS_SYSCALL_RAX
    cmp rax, 0x1000
    jl Lno_syscall
    sub rax, 0x1000
    cmp rax, NUM_SYSCALLS
    jge Lno_syscall

    shl rax, 3
    lea rcx, [rip+syscalls]
    add rax, rcx
    mov rax, [rax]

    mov rcx, r10

    sti
    call rax
    cli

Lafter_syscall:
    push rax
    call acquire_sync_lock

    mov rcx, [rsp]
    call maybe_yield

    call release_sync_lock
    pop rax

    # mark thread as running user code
    mov rdx, gs:GS_CURRENT_THREAD
    mov BYTE PTR [rdx + THREAD_KERNEL_MODE], 0

    mov rcx, 0x33 # ss
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RSP]
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_EFLAGS]
    mov rcx, 0x3b # cs
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RIP]

    # load process cr3
    mov rcx, [rdx + THREAD_PROC]
    mov rcx, [rcx + PROCESS_CR3]
    mov cr3, rcx

    swapgs

    # don't leak information to userspace
    xor rcx, rcx
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11

    iretq

Lno_syscall:
    mov rax, 0xffffffffc0000002 # STATUS_NOT_IMPLEMENTED
    jmp Lafter_syscall

syscall_handler_end:

.text_end:
