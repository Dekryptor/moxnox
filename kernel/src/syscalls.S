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
    .section .rdata
    .quad "stub_\name"
.endm

.macro syscall_func name, args
    .quad "user_\name"
.endm

.section .rdata
syscalls:
    syscall_func "NtDisplayString", 1
    syscall_func "NtTerminateThread", 2
    syscall_func "NtTerminateProcess", 2
    syscall_func "NtWriteFile", 9
    syscall_func "NtResumeProcess", 1
    syscall_func "NtAllocateVirtualMemory", 6
    syscall_func "NtFreeVirtualMemory", 4
    syscall_func "NtReadFile", 9
    syscall_func "NtProtectVirtualMemory", 5
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
    syscall_func "NtCancelTimer", 2
    syscall_stub "NtClearEvent"
    syscall_func "NtClose", 1
    syscall_stub "NtCloseObjectAuditAlarm"
    syscall_stub "NtCompleteConnectPort"
    syscall_stub "NtConnectPort"
    syscall_stub "NtCreateDirectoryObject"
    syscall_func "NtCreateEvent", 5
    syscall_stub "NtCreateEventPair"
    syscall_func "NtCreateFile", 11
    syscall_stub "NtCreateIoCompletion"
    syscall_stub "NtCreateKey"
    syscall_stub "NtCreateMailslotFile"
    syscall_stub "NtCreateMutant"
    syscall_func "NtCreateNamedPipeFile", 14
    syscall_stub "NtCreatePagingFile"
    syscall_stub "NtCreatePort"
    syscall_func "NtCreateProcess", 8
    syscall_stub "NtCreateProfile"
    syscall_func "NtCreateSection", 7
    syscall_stub "NtCreateSemaphore"
    syscall_stub "NtCreateSymbolicLinkObject"
    syscall_func "NtCreateThread", 8
    syscall_func "NtCreateTimer", 4
    syscall_stub "NtCreateToken"
    syscall_func "NtDelayExecution", 2
    syscall_stub "NtDeleteAtom"
    syscall_stub "NtDeleteFile"
    syscall_stub "NtDeleteKey"
    syscall_stub "NtDeleteObjectAuditAlarm"
    syscall_stub "NtDeleteValueKey"
    syscall_func "NtDeviceIoControlFile", 10
    syscall_func "NtDuplicateObject", 7
    syscall_stub "NtDuplicateToken"
    syscall_stub "NtEnumerateKey"
    syscall_stub "NtEnumerateValueKey"
    syscall_stub "NtExtendSection"
    syscall_stub "NtFindAtom"
    syscall_stub "NtFlushBuffersFile"
    syscall_stub "NtFlushInstructionCache"
    syscall_stub "NtFlushKey"
    syscall_func "NtFlushVirtualMemory", 4
    syscall_stub "NtFlushWriteBuffer"
    syscall_stub "NtFsControlFile"
    syscall_func "NtGetContextThread", 2
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
    syscall_func "NtMapViewOfSection", 10
    syscall_stub "NtNotifyChangeDirectoryFile"
    syscall_stub "NtNotifyChangeKey"
    syscall_stub "NtOpenDirectoryObject"
    syscall_stub "NtOpenEvent"
    syscall_stub "NtOpenEventPair"
    syscall_func "NtOpenFile", 6
    syscall_stub "NtOpenIoCompletion"
    syscall_stub "NtOpenKey"
    syscall_stub "NtOpenMutant"
    syscall_stub "NtOpenObjectAuditAlarm"
    syscall_func "NtOpenProcess", 4
    syscall_stub "NtOpenProcessToken"
    syscall_func "NtOpenSection", 3
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
    syscall_func "NtQueryDirectoryFile", 11
    syscall_stub "NtQueryDirectoryObject"
    syscall_stub "NtQueryEaFile"
    syscall_stub "NtQueryEvent"
    syscall_stub "NtQueryFullAttributesFile"
    syscall_stub "NtQueryInformationAtom"
    syscall_func "NtQueryInformationFile", 5
    syscall_stub "NtQueryInformationPort"
    syscall_func "NtQueryInformationProcess", 5
    syscall_func "NtQueryInformationThread", 5
    syscall_stub "NtQueryInformationToken"
    syscall_stub "NtQueryIntervalProfile"
    syscall_stub "NtQueryIoCompletion"
    syscall_stub "NtQueryKey"
    syscall_stub "NtQueryMultipleValueKey"
    syscall_stub "NtQueryMutant"
    syscall_stub "NtQueryObject"
    syscall_stub "NtQueryPerformanceCounter"
    syscall_func "NtQuerySection", 5
    syscall_stub "NtQuerySecurityObject"
    syscall_stub "NtQuerySemaphore"
    syscall_stub "NtQuerySymbolicLinkObject"
    syscall_stub "NtQuerySystemEnvironmentValue"
    syscall_stub "NtQuerySystemInformation"
    syscall_func "NtQuerySystemTime", 1
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
    syscall_func "NtReadVirtualMemory", 5
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
    syscall_func "NtResumeThread", 2
    syscall_stub "NtSaveKey"
    syscall_func "NtSetContextThread", 2
    syscall_stub "NtSetDefaultHardErrorPort"
    syscall_stub "NtSetDefaultLocale"
    syscall_stub "NtSetEaFile"
    syscall_func "NtSetEvent", 2
    syscall_stub "NtSetHighEventPair"
    syscall_stub "NtSetHighWaitLowEventPair"
    syscall_func "NtSetInformationFile", 5
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
    syscall_func "NtSetTimer", 7
    syscall_stub "NtSetTimerResolution"
    syscall_stub "NtSetValueKey"
    syscall_stub "NtSetVolumeInformationFile"
    syscall_func "NtShutdownSystem", 1
    syscall_stub "NtSignalAndWaitForSingleObject"
    syscall_stub "NtStartProfile"
    syscall_stub "NtStopProfile"
    syscall_func "NtSuspendThread", 2
    syscall_stub "NtSystemDebugControl"
    syscall_stub "NtTestAlert"
    syscall_stub "NtUnloadDriver"
    syscall_stub "NtUnloadKey"
    syscall_stub "NtUnlockFile"
    syscall_stub "NtUnlockVirtualMemory"
    syscall_func "NtUnmapViewOfSection", 2
    syscall_stub "NtVdmControl"
    syscall_func "NtWaitForMultipleObjects", 5
    syscall_func "NtWaitForSingleObject", 3
    syscall_stub "NtWaitHighEventPair"
    syscall_stub "NtWaitLowEventPair"
    syscall_stub "NtWriteFileGather"
    syscall_stub "NtWriteRequestData"
    syscall_func "NtWriteVirtualMemory", 5
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
    syscall_func "NtGetNextProcess", 5
    syscall_func "NtGetNextThread", 6
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
    syscall_func "NtSuspendProcess", 1
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
