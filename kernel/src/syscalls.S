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
syscall_stub "NtCreateNamedPipeFile"
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
syscall_stub "NtOpenFile"
syscall_stub "NtOpenIoCompletion"
syscall_stub "NtOpenKey"
syscall_stub "NtOpenMutant"
syscall_stub "NtOpenObjectAuditAlarm"
syscall_stub "NtOpenProcess"
syscall_stub "NtOpenProcessToken"
syscall_stub "NtOpenSection"
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
syscall_stub "NtWaitForMultipleObjects"
syscall_stub "NtWaitHighEventPair"
syscall_stub "NtWaitLowEventPair"
syscall_stub "NtWriteFileGather"
syscall_stub "NtWriteRequestData"
syscall_stub "NtYieldExecution"

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
end_syscalls:

# FIXME - Clang forces the nice version to be a memory address (GAS is okay)
# .equ NUM_SYSCALLS, (end_syscalls - syscalls) / 8
.equ NUM_SYSCALLS, 0xc8

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
