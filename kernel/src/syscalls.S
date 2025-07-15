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
    .global "user_\name"
    "user_\name":
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
syscall_stub "NtCreateEvent"
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
syscall_stub "NtDelayExecution"
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
syscall_stub "NtGetContextThread"
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
syscall_stub "NtOpenProcess"
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
syscall_stub "NtSetContextThread"
syscall_stub "NtSetDefaultHardErrorPort"
syscall_stub "NtSetDefaultLocale"
syscall_stub "NtSetEaFile"
syscall_stub "NtSetEvent"
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
syscall_stub "NtSuspendThread"
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
syscall_stub "NtGetNextProcess"
syscall_stub "NtGetNextThread"
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
syscall_stub "NtSuspendProcess"
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
    .quad user_NtAcceptConnectPort
    .quad user_NtAccessCheck
    .quad user_NtAccessCheckAndAuditAlarm
    .quad user_NtAddAtom
    .quad user_NtAdjustGroupsToken
    .quad user_NtAdjustPrivilegesToken
    .quad user_NtAlertResumeThread
    .quad user_NtAlertThread
    .quad user_NtAllocateLocallyUniqueId
    .quad user_NtAllocateUuids
    .quad user_NtCallbackReturn
    .quad user_NtCancelIoFile
    .quad user_NtCancelTimer
    .quad user_NtClearEvent
    .quad user_NtClose
    .quad user_NtCloseObjectAuditAlarm
    .quad user_NtCompleteConnectPort
    .quad user_NtConnectPort
    .quad user_NtCreateDirectoryObject
    .quad user_NtCreateEvent
    .quad user_NtCreateEventPair
    .quad user_NtCreateFile
    .quad user_NtCreateIoCompletion
    .quad user_NtCreateKey
    .quad user_NtCreateMailslotFile
    .quad user_NtCreateMutant
    .quad user_NtCreateNamedPipeFile
    .quad user_NtCreatePagingFile
    .quad user_NtCreatePort
    .quad user_NtCreateProcess
    .quad user_NtCreateProfile
    .quad user_NtCreateSection
    .quad user_NtCreateSemaphore
    .quad user_NtCreateSymbolicLinkObject
    .quad user_NtCreateThread
    .quad user_NtCreateTimer
    .quad user_NtCreateToken
    .quad user_NtDelayExecution
    .quad user_NtDeleteAtom
    .quad user_NtDeleteFile
    .quad user_NtDeleteKey
    .quad user_NtDeleteObjectAuditAlarm
    .quad user_NtDeleteValueKey
    .quad user_NtDeviceIoControlFile
    .quad user_NtDuplicateObject
    .quad user_NtDuplicateToken
    .quad user_NtEnumerateKey
    .quad user_NtEnumerateValueKey
    .quad user_NtExtendSection
    .quad user_NtFindAtom
    .quad user_NtFlushBuffersFile
    .quad user_NtFlushInstructionCache
    .quad user_NtFlushKey
    .quad user_NtFlushVirtualMemory
    .quad user_NtFlushWriteBuffer
    .quad user_NtFsControlFile
    .quad user_NtGetContextThread
    .quad user_NtImpersonateClientOfPort
    .quad user_NtImpersonateThread
    .quad user_NtInitializeRegistry
    .quad user_NtListenPort
    .quad user_NtLoadDriver
    .quad user_NtLoadKey
    .quad user_NtLoadKey2
    .quad user_NtLockFile
    .quad user_NtLockVirtualMemory
    .quad user_NtMakeTemporaryObject
    .quad user_NtMapViewOfSection
    .quad user_NtNotifyChangeDirectoryFile
    .quad user_NtNotifyChangeKey
    .quad user_NtOpenDirectoryObject
    .quad user_NtOpenEvent
    .quad user_NtOpenEventPair
    .quad user_NtOpenFile
    .quad user_NtOpenIoCompletion
    .quad user_NtOpenKey
    .quad user_NtOpenMutant
    .quad user_NtOpenObjectAuditAlarm
    .quad user_NtOpenProcess
    .quad user_NtOpenProcessToken
    .quad user_NtOpenSection
    .quad user_NtOpenSemaphore
    .quad user_NtOpenSymbolicLinkObject
    .quad user_NtOpenThread
    .quad user_NtOpenThreadToken
    .quad user_NtOpenTimer
    .quad user_NtPlugPlayControl
    .quad user_NtPrivilegeCheck
    .quad user_NtPrivilegeObjectAuditAlarm
    .quad user_NtPrivilegedServiceAuditAlarm
    .quad user_NtPulseEvent
    .quad user_NtQueryAttributesFile
    .quad user_NtQueryDefaultLocale
    .quad user_NtQueryDirectoryFile
    .quad user_NtQueryDirectoryObject
    .quad user_NtQueryEaFile
    .quad user_NtQueryEvent
    .quad user_NtQueryFullAttributesFile
    .quad user_NtQueryInformationAtom
    .quad user_NtQueryInformationFile
    .quad user_NtQueryInformationPort
    .quad user_NtQueryInformationProcess
    .quad user_NtQueryInformationThread
    .quad user_NtQueryInformationToken
    .quad user_NtQueryIntervalProfile
    .quad user_NtQueryIoCompletion
    .quad user_NtQueryKey
    .quad user_NtQueryMultipleValueKey
    .quad user_NtQueryMutant
    .quad user_NtQueryObject
    .quad user_NtQueryPerformanceCounter
    .quad user_NtQuerySection
    .quad user_NtQuerySecurityObject
    .quad user_NtQuerySemaphore
    .quad user_NtQuerySymbolicLinkObject
    .quad user_NtQuerySystemEnvironmentValue
    .quad user_NtQuerySystemInformation
    .quad user_NtQuerySystemTime
    .quad user_NtQueryTimer
    .quad user_NtQueryTimerResolution
    .quad user_NtQueryValueKey
    .quad user_NtQueryVirtualMemory
    .quad user_NtQueryVolumeInformationFile
    .quad user_NtQueueApcThread
    .quad user_NtRaiseException
    .quad user_NtRaiseHardError
    .quad user_NtReadFileScatter
    .quad user_NtReadRequestData
    .quad user_NtReadVirtualMemory
    .quad user_NtRegisterThreadTerminatePort
    .quad user_NtReleaseMutant
    .quad user_NtReleaseSemaphore
    .quad user_NtRemoveIoCompletion
    .quad user_NtReplaceKey
    .quad user_NtReplyPort
    .quad user_NtReplyWaitReceivePort
    .quad user_NtReplyWaitReplyPort
    .quad user_NtRequestPort
    .quad user_NtRequestWaitReplyPort
    .quad user_NtResetEvent
    .quad user_NtRestoreKey
    .quad user_NtResumeThread
    .quad user_NtSaveKey
    .quad user_NtSetContextThread
    .quad user_NtSetDefaultHardErrorPort
    .quad user_NtSetDefaultLocale
    .quad user_NtSetEaFile
    .quad user_NtSetEvent
    .quad user_NtSetHighEventPair
    .quad user_NtSetHighWaitLowEventPair
    .quad user_NtSetInformationFile
    .quad user_NtSetInformationKey
    .quad user_NtSetInformationObject
    .quad user_NtSetInformationProcess
    .quad user_NtSetInformationThread
    .quad user_NtSetInformationToken
    .quad user_NtSetIntervalProfile
    .quad user_NtSetIoCompletion
    .quad user_NtSetLdtEntries
    .quad user_NtSetLowEventPair
    .quad user_NtSetLowWaitHighEventPair
    .quad user_NtSetSecurityObject
    .quad user_NtSetSystemEnvironmentValue
    .quad user_NtSetSystemInformation
    .quad user_NtSetSystemPowerState
    .quad user_NtSetSystemTime
    .quad user_NtSetTimer
    .quad user_NtSetTimerResolution
    .quad user_NtSetValueKey
    .quad user_NtSetVolumeInformationFile
    .quad user_NtShutdownSystem
    .quad user_NtSignalAndWaitForSingleObject
    .quad user_NtStartProfile
    .quad user_NtStopProfile
    .quad user_NtSuspendThread
    .quad user_NtSystemDebugControl
    .quad user_NtTestAlert
    .quad user_NtUnloadDriver
    .quad user_NtUnloadKey
    .quad user_NtUnlockFile
    .quad user_NtUnlockVirtualMemory
    .quad user_NtUnmapViewOfSection
    .quad user_NtVdmControl
    .quad user_NtWaitForMultipleObjects
    .quad user_NtWaitForSingleObject
    .quad user_NtWaitHighEventPair
    .quad user_NtWaitLowEventPair
    .quad user_NtWriteFileGather
    .quad user_NtWriteRequestData
    .quad user_NtWriteVirtualMemory
    .quad user_NtYieldExecution
    .quad user_NtAccessCheckByType
    .quad user_NtAccessCheckByTypeAndAuditAlarm
    .quad user_NtAccessCheckByTypeResultList
    .quad user_NtAccessCheckByTypeResultListAndAuditAlarm
    .quad user_NtAccessCheckByTypeResultListAndAuditAlarmByHandle
    .quad user_NtAcquireCMFViewOwnership
    .quad user_NtAddBootEntry
    .quad user_NtAddDriverEntry
    .quad user_NtAllocateUserPhysicalPages
    .quad user_NtAlpcAcceptConnectPort
    .quad user_NtAlpcCancelMessage
    .quad user_NtAlpcConnectPort
    .quad user_NtAlpcCreatePort
    .quad user_NtAlpcCreatePortSection
    .quad user_NtAlpcCreateResourceReserve
    .quad user_NtAlpcCreateSectionView
    .quad user_NtAlpcCreateSecurityContext
    .quad user_NtAlpcDeletePortSection
    .quad user_NtAlpcDeleteResourceReserve
    .quad user_NtAlpcDeleteSectionView
    .quad user_NtAlpcDeleteSecurityContext
    .quad user_NtAlpcDisconnectPort
    .quad user_NtAlpcImpersonateClientOfPort
    .quad user_NtAlpcOpenSenderProcess
    .quad user_NtAlpcOpenSenderThread
    .quad user_NtAlpcQueryInformation
    .quad user_NtAlpcQueryInformationMessage
    .quad user_NtAlpcRevokeSecurityContext
    .quad user_NtAlpcSendWaitReceivePort
    .quad user_NtAlpcSetInformation
    .quad user_NtApphelpCacheControl
    .quad user_NtAreMappedFilesTheSame
    .quad user_NtAssignProcessToJobObject
    .quad user_NtCancelDeviceWakeupRequest
    .quad user_NtCancelIoFileEx
    .quad user_NtCancelSynchronousIoFile
    .quad user_NtClearAllSavepointsTransaction
    .quad user_NtClearSavepointTransaction
    .quad user_NtCommitComplete
    .quad user_NtCommitEnlistment
    .quad user_NtCommitTransaction
    .quad user_NtCompactKeys
    .quad user_NtCompareTokens
    .quad user_NtCompressKey
    .quad user_NtCreateDebugObject
    .quad user_NtCreateEnlistment
    .quad user_NtCreateJobObject
    .quad user_NtCreateJobSet
    .quad user_NtCreateKeyedEvent
    .quad user_NtCreateKeyTransacted
    .quad user_NtCreatePrivateNamespace
    .quad user_NtCreateProcessEx
    .quad user_NtCreateResourceManager
    .quad user_NtCreateThreadEx
    .quad user_NtCreateTransaction
    .quad user_NtCreateTransactionManager
    .quad user_NtCreateUserProcess
    .quad user_NtCreateWaitablePort
    .quad user_NtCreateWorkerFactory
    .quad user_NtDebugActiveProcess
    .quad user_NtDebugContinue
    .quad user_NtDeleteBootEntry
    .quad user_NtDeleteDriverEntry
    .quad user_NtDeletePrivateNamespace
    .quad user_NtEnumerateBootEntries
    .quad user_NtEnumerateDriverEntries
    .quad user_NtEnumerateSystemEnvironmentValuesEx
    .quad user_NtEnumerateTransactionObject
    .quad user_NtFilterToken
    .quad user_NtFlushInstallUILanguage
    .quad user_NtFlushProcessWriteBuffers
    .quad user_NtFreeUserPhysicalPages
    .quad user_NtFreezeRegistry
    .quad user_NtFreezeTransactions
    .quad user_NtGetCurrentProcessorNumber
    .quad user_NtGetDevicePowerState
    .quad user_NtGetMUIRegistryInfo
    .quad user_NtGetNextProcess
    .quad user_NtGetNextThread
    .quad user_NtGetNlsSectionPtr
    .quad user_NtGetNotificationResourceManager
    .quad user_NtGetPlugPlayEvent
    .quad user_NtGetWriteWatch
    .quad user_NtImpersonateAnonymousToken
    .quad user_NtInitializeNlsFiles
    .quad user_NtInitiatePowerAction
    .quad user_NtIsProcessInJob
    .quad user_NtIsSystemResumeAutomatic
    .quad user_NtIsUILanguageComitted
    .quad user_NtListTransactions
    .quad user_NtLoadKeyEx
    .quad user_NtLockProductActivationKeys
    .quad user_NtLockRegistryKey
    .quad user_NtMakePermanentObject
    .quad user_NtMapCMFModule
    .quad user_NtMapUserPhysicalPages
    .quad user_NtMapUserPhysicalPagesScatter
    .quad user_NtMarshallTransaction
    .quad user_NtModifyBootEntry
    .quad user_NtModifyDriverEntry
    .quad user_NtNotifyChangeMultipleKeys
    .quad user_NtOpenEnlistment
    .quad user_NtOpenJobObject
    .quad user_NtOpenKeyedEvent
    .quad user_NtOpenKeyTransacted
    .quad user_NtOpenPrivateNamespace
    .quad user_NtOpenProcessTokenEx
    .quad user_NtOpenResourceManager
    .quad user_NtOpenSession
    .quad user_NtOpenThreadTokenEx
    .quad user_NtOpenTransaction
    .quad user_NtOpenTransactionManager
    .quad user_NtPowerInformation
    .quad user_NtPrepareComplete
    .quad user_NtPrepareEnlistment
    .quad user_NtPrePrepareComplete
    .quad user_NtPrePrepareEnlistment
    .quad user_NtPropagationComplete
    .quad user_NtPropagationFailed
    .quad user_NtPullTransaction
    .quad user_NtQueryBootEntryOrder
    .quad user_NtQueryBootOptions
    .quad user_NtQueryDebugFilterState
    .quad user_NtQueryDefaultUILanguage
    .quad user_NtQueryDriverEntryOrder
    .quad user_NtQueryInformationEnlistment
    .quad user_NtQueryInformationJobObject
    .quad user_NtQueryInformationResourceManager
    .quad user_NtQueryInformationTransaction
    .quad user_NtQueryInformationTransactionManager
    .quad user_NtQueryInformationWorkerFactory
    .quad user_NtQueryInstallUILanguage
    .quad user_NtQueryLicenseValue
    .quad user_NtQueryOpenSubKeys
    .quad user_NtQueryOpenSubKeysEx
    .quad user_NtQueryPortInformationProcess
    .quad user_NtQueryQuotaInformationFile
    .quad user_NtQuerySystemEnvironmentValueEx
    .quad user_NtReadOnlyEnlistment
    .quad user_NtRecoverEnlistment
    .quad user_NtRecoverResourceManager
    .quad user_NtRecoverTransactionManager
    .quad user_NtRegisterProtocolAddressInformation
    .quad user_NtReleaseCMFViewOwnership
    .quad user_NtReleaseKeyedEvent
    .quad user_NtReleaseWorkerFactoryWorker
    .quad user_NtRemoveIoCompletionEx
    .quad user_NtRemoveProcessDebug
    .quad user_NtRenameKey
    .quad user_NtRenameTransactionManager
    .quad user_NtReplacePartitionUnit
    .quad user_NtReplyWaitReceivePortEx
    .quad user_NtRequestDeviceWakeup
    .quad user_NtRequestWakeupLatency
    .quad user_NtResetWriteWatch
    .quad user_NtRollbackComplete
    .quad user_NtRollbackEnlistment
    .quad user_NtRollbackSavepointTransaction
    .quad user_NtRollbackTransaction
    .quad user_NtRollforwardTransactionManager
    .quad user_NtSaveKeyEx
    .quad user_NtSaveMergedKeys
    .quad user_NtSavepointComplete
    .quad user_NtSavepointTransaction
    .quad user_NtSecureConnectPort
    .quad user_NtSetBootEntryOrder
    .quad user_NtSetBootOptions
    .quad user_NtSetDebugFilterState
    .quad user_NtSetDefaultUILanguage
    .quad user_NtSetDriverEntryOrder
    .quad user_NtSetEventBoostPriority
    .quad user_NtSetInformationDebugObject
    .quad user_NtSetInformationEnlistment
    .quad user_NtSetInformationJobObject
    .quad user_NtSetInformationResourceManager
    .quad user_NtSetInformationTransaction
    .quad user_NtSetInformationTransactionManager
    .quad user_NtSetInformationWorkerFactory
    .quad user_NtSetQuotaInformationFile
    .quad user_NtSetSystemEnvironmentValueEx
    .quad user_NtSetThreadExecutionState
    .quad user_NtSetUuidSeed
    .quad user_NtShutdownWorkerFactory
    .quad user_NtSinglePhaseReject
    .quad user_NtStartTm
    .quad user_NtSuspendProcess
    .quad user_NtTerminateJobObject
    .quad user_NtThawRegistry
    .quad user_NtThawTransactions
    .quad user_NtTraceControl
    .quad user_NtTraceEvent
    .quad user_NtTranslateFilePath
    .quad user_NtUnloadKey2
    .quad user_NtUnloadKeyEx
    .quad user_NtWaitForDebugEvent
    .quad user_NtWaitForKeyedEvent
    .quad user_NtWaitForMultipleObjects32
    .quad user_NtWaitForWorkViaWorkerFactory
    .quad user_NtWorkerFactoryWorkerReady
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
    # check for pending signal
    mov rdx, gs:GS_CURRENT_THREAD
    cmp QWORD PTR [rdx + THREAD_PENDING_SIGNAL], 0
    jne Ldispatch_signal

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

Ldispatch_signal:
    call dispatch_signal

    mov rdx, gs:GS_CURRENT_THREAD
    mov QWORD PTR [rdx + THREAD_PENDING_SIGNAL], 0

    # mark thread as running user code
    mov BYTE PTR [rdx + THREAD_KERNEL_MODE], 0

    mov rcx, 0x33 # ss
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RSP]
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_EFLAGS]
    mov rcx, 0x3b # cs
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RIP]

    mov rcx, [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RCX]
    mov r8, [rdx + THREAD_PROC]
    mov rdx, [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RDX]

    # load process cr3
    mov r8, [r8 + PROCESS_CR3]
    mov cr3, r8

    swapgs

    # don't leak information to userspace
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
