module;

#include <stdint.h>

export module ntdll;

import nt;

export {

using PRTL_HEAP_COMMIT_ROUTINE = NTSTATUS (*)(const void* base, void** commit_address,
                                              size_t* commit_size);

struct RTL_HEAP_PARAMETERS {
    uint32_t Length;
    size_t SegmentReserve;
    size_t SegmentCommit;
    size_t DeCommitFreeBlockThreshold;
    size_t DeCommitTotalFreeThreshold;
    size_t MaximumAllocationSize;
    size_t VirtualMemoryThreshold;
    size_t InitialCommit;
    size_t InitialReserve;
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
    size_t Reserved[2];
};

const uint32_t HEAP_NO_SERIALIZE = 0x00000001;
const uint32_t HEAP_GROWABLE  = 0x00000002;
const uint32_t HEAP_GENERATE_EXCEPTIONS = 0x00000004;
const uint32_t HEAP_ZERO_MEMORY = 0x00000008;
const uint32_t HEAP_REALLOC_IN_PLACE_ONLY = 0x00000010;
const uint32_t HEAP_TAIL_CHECKING_ENABLED = 0x00000020;
const uint32_t HEAP_FREE_CHECKING_ENABLED = 0x00000040;
const uint32_t HEAP_DISABLE_COALESCE_ON_FREE = 0x00000080;
const uint32_t HEAP_CREATE_ALIGN_16 = 0x00010000;
const uint32_t HEAP_CREATE_ENABLE_TRACING = 0x00020000;
const uint32_t HEAP_CREATE_ENABLE_EXECUTE = 0x00040000;

struct LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    void* DllBase;
    void* EntryPoint;
    uint32_t SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    union {
        uint32_t Flags;
        struct {
            uint32_t PackagedBinary : 1;
            uint32_t MarkedForRemoval : 1;
            uint32_t ImageDll : 1;
            uint32_t LoadNotificationsSent : 1;
            uint32_t TelemetryEntryProcessed : 1;
            uint32_t ProcessStaticImport : 1;
            uint32_t InLegacyLists : 1;
            uint32_t InIndexes : 1;
            uint32_t ShimDll : 1;
            uint32_t InExceptionTable : 1;
            uint32_t ReservedFlags1 : 2;
            uint32_t LoadInProgress : 1;
            uint32_t LoadConfigProcessed : 1;
            uint32_t EntryProcessed : 1;
            uint32_t ProtectDelayLoad : 1;
            uint32_t ReservedFlags3 : 2;
            uint32_t DontCallForThreads : 1;
            uint32_t ProcessAttachCalled : 1;
            uint32_t ProcessAttachFailed : 1;
            uint32_t CorDeferredValidate : 1;
            uint32_t CorImage : 1;
            uint32_t DontRelocate : 1;
            uint32_t CorILOnly : 1;
            uint32_t ChpeImage : 1;
            uint32_t ReservedFlags5 : 2;
            uint32_t Redirected : 1;
            uint32_t ReservedFlags6 : 2;
            uint32_t CompatDatabaseProcessed : 1;
        };
    };
    uint16_t LoadCount;
    uint16_t TlsIndex;
    LIST_ENTRY HashLinks;
    uint32_t TimeDateStamp;
};

const uint32_t DLL_PROCESS_DETACH = 0;
const uint32_t DLL_PROCESS_ATTACH = 1;
const uint32_t DLL_THREAD_ATTACH = 2;
const uint32_t DLL_THREAD_DETACH = 3;

struct mn_image_info {
    void* base;
    uint64_t offset_debug_info;
    uint64_t size_debug_info;
    uint64_t offset_debug_abbrev;
    uint64_t size_debug_abbrev;
    uint64_t offset_debug_rnglists;
    uint64_t size_debug_rnglists;
    uint64_t offset_debug_str;
    uint64_t size_debug_str;
    uint64_t offset_debug_addr;
    uint64_t size_debug_addr;
    uint64_t offset_debug_str_offsets;
    uint64_t size_debug_str_offsets;
    uint64_t offset_debug_line;
    uint64_t size_debug_line;
    uint64_t offset_debug_line_str;
    uint64_t size_debug_line_str;
};

enum class EXCEPTION_DISPOSITION : int {
    ExceptionContinueExecution = 0,
    ExceptionContinueSearch = 1,
    ExceptionNestedException = 2,
    ExceptionCollidedUnwind = 3,
    ExceptionExecuteHandler = 4
};

const int32_t EXCEPTION_EXECUTE_HANDLER = 1;
const int32_t EXCEPTION_CONTINUE_SEARCH = 0;
const int32_t EXCEPTION_CONTINUE_EXECUTION = -1;

const uint32_t EH_NONCONTINUABLE = 0x00000001;
const uint32_t EH_UNWINDING = 0x00000002;
const uint32_t EH_EXIT_UNWIND = 0x00000004;
const uint32_t EH_STACK_INVALID = 0x00000008;
const uint32_t EH_NESTED_CALL = 0x00000010;
const uint32_t EH_TARGET_UNWIND = 0x00000020;
const uint32_t EH_COLLIDED_UNWIND = 0x00000040;

using EXCEPTION_ROUTINE = EXCEPTION_DISPOSITION(*)(EXCEPTION_RECORD*, void*,
                                                   CONTEXT*, void*);

struct EXCEPTION_POINTERS {
    EXCEPTION_RECORD* ExceptionRecord;
    CONTEXT* ContextRecord;
};

struct SCOPE_TABLE {
    uint32_t Count;
    struct {
        uint32_t BeginAddress;
        uint32_t EndAddress;
        uint32_t HandlerAddress;
        uint32_t JumpTarget;
    } ScopeRecord[1];
};

#ifdef __x86_64__
using EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_pointers,
                                     void* establisher_frame);
using TERMINATION_HANDLER = void (*)(bool abnormal_termination,
                                     void* establisher_frame);
#elifdef __aarch64__
using EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_pointers,
                                     uint64_t establisher_frame);
using TERMINATION_HANDLER = void (*)(bool abnormal_termination,
                                     uint64_t establisher_frame);
#endif

struct ANSI_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char* Buffer;
};

struct RTL_CONDITION_VARIABLE {
    void* Ptr;
};

struct RTL_SRWLOCK {
    void* Ptr;
};

#ifdef __x86_64__

const size_t UNWIND_HISTORY_TABLE_SIZE = 12;

struct UNWIND_HISTORY_TABLE_ENTRY {
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
};

struct UNWIND_HISTORY_TABLE {
    uint32_t Count;
    uint8_t Search;
    uint64_t LowAddress;
    uint64_t HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
};

struct KNONVOLATILE_CONTEXT_POINTERS {
    union {
        M128A* FloatingContext[16];
        struct {
            M128A* Xmm0;
            M128A* Xmm1;
            M128A* Xmm2;
            M128A* Xmm3;
            M128A* Xmm4;
            M128A* Xmm5;
            M128A* Xmm6;
            M128A* Xmm7;
            M128A* Xmm8;
            M128A* Xmm9;
            M128A* Xmm10;
            M128A* Xmm11;
            M128A* Xmm12;
            M128A* Xmm13;
            M128A* Xmm14;
            M128A* Xmm15;
        };
    };
    union {
        uint64_t* IntegerContext[16];
        struct {
            uint64_t* Rax;
            uint64_t* Rcx;
            uint64_t* Rdx;
            uint64_t* Rbx;
            uint64_t* Rsp;
            uint64_t* Rbp;
            uint64_t* Rsi;
            uint64_t* Rdi;
            uint64_t* R8;
            uint64_t* R9;
            uint64_t* R10;
            uint64_t* R11;
            uint64_t* R12;
            uint64_t* R13;
            uint64_t* R14;
            uint64_t* R15;
        };
    };
};

struct DISPATCHER_CONTEXT_AMD64 {
    uint64_t ControlPc;
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
    uint64_t EstablisherFrame;
    uint64_t TargetIp;
    AMD64_CONTEXT* ContextRecord;
    EXCEPTION_ROUTINE LanguageHandler;
    void* HandlerData;
    UNWIND_HISTORY_TABLE* HistoryTable;
    uint32_t ScopeIndex;
    uint32_t Fill0;
};

using DISPATCHER_CONTEXT = DISPATCHER_CONTEXT_AMD64;

#elifdef __aarch64__

const size_t UNWIND_HISTORY_TABLE_SIZE = 12;

struct UNWIND_HISTORY_TABLE_ENTRY {
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
};

struct UNWIND_HISTORY_TABLE {
    uint32_t Count;
    uint8_t LocalHint;
    uint8_t GlobalHint;
    uint8_t Search;
    uint8_t Once;
    uint64_t LowAddress;
    uint64_t HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
};

struct KNONVOLATILE_CONTEXT_POINTERS {
    uint64_t* X19;
    uint64_t* X20;
    uint64_t* X21;
    uint64_t* X22;
    uint64_t* X23;
    uint64_t* X24;
    uint64_t* X25;
    uint64_t* X26;
    uint64_t* X27;
    uint64_t* X28;
    uint64_t* Fp;
    uint64_t* Lr;
    uint64_t* D8;
    uint64_t* D9;
    uint64_t* D10;
    uint64_t* D11;
    uint64_t* D12;
    uint64_t* D13;
    uint64_t* D14;
    uint64_t* D15;
};

struct DISPATCHER_CONTEXT_ARM64 {
    uintptr_t ControlPc;
    uintptr_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
    uintptr_t EstablisherFrame;
    uintptr_t TargetPc;
    ARM64_NT_CONTEXT* ContextRecord;
    EXCEPTION_ROUTINE LanguageHandler;
    void* HandlerData;
    UNWIND_HISTORY_TABLE* HistoryTable;
    uint32_t ScopeIndex;
    bool ControlPcIsUnwound; // FIXME?
    uint8_t* NonVolatileRegisters;
};

using DISPATCHER_CONTEXT = DISPATCHER_CONTEXT_ARM64;

#endif

extern "C" {

void* RtlCreateHeap(uint32_t flags, void* heap_base, size_t reserve_size, size_t commit_size,
                    void* lock, RTL_HEAP_PARAMETERS* parameters);
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size);
void* RtlReAllocateHeap(void* heap_handle, uint32_t flags, void* ptr,
                        size_t size);
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address);
void* RtlDestroyHeap(void* heap_handle);
void RtlRaiseException(EXCEPTION_RECORD* exception_record);
void RtlCaptureContext(CONTEXT* context);
NTSTATUS LdrGetProcedureAddress(void* dll_handle, const ANSI_STRING* procedure_name,
                                uint32_t procedure_number, void** procedure_address);
EXCEPTION_DISPOSITION __C_specific_handler(EXCEPTION_RECORD* exception_record,
                                           void* establisher_frame,
                                           CONTEXT* context_record,
                                           DISPATCHER_CONTEXT* dispatcher_context);
void RtlExitUserProcess(uint32_t status);
nt_status RtlInitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlDeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlEnterCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlLeaveCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void RtlSetLastWin32Error(uint32_t err_code);
uint32_t RtlNtStatusToDosErrorNoTeb(nt_status status);
uint32_t RtlNtStatusToDosError(nt_status status);
void RtlInitializeConditionVariable(RTL_CONDITION_VARIABLE* var);
void RtlWakeAllConditionVariable(RTL_CONDITION_VARIABLE* var);
void RtlExitUserThread(nt_status exit_status) __attribute__((noreturn));
void RtlAcquirePebLock();
void RtlReleasePebLock();
void RtlAcquireLoaderLock();
void RtlReleaseLoaderLock();
nt_status RtlSleepConditionVariableCS(RTL_CONDITION_VARIABLE* variable,
                                      RTL_CRITICAL_SECTION* crit,
                                      const LARGE_INTEGER* timeout);
void RtlWakeConditionVariable(RTL_CONDITION_VARIABLE* variable);
nt_status RtlWaitOnAddress(const void* addr, const void* cmp, size_t size,
                           const LARGE_INTEGER* timeout);
void RtlWakeConditionVariable(RTL_CONDITION_VARIABLE* variable);
void RtlWakeAddressSingle(const void* addr);
void RtlWakeAddressAll(const void* addr);
void* RtlPcToFileHeader(const void* pc, void** address);
nt_status LdrAddRefDll(uint32_t flags, HMODULE mod);
nt_status LdrUnloadDll(HMODULE mod);
void RtlAcquireSRWLockExclusive(RTL_SRWLOCK* lock);

void RtlUnwindEx(void* target_frame, void* target_ip,
                 EXCEPTION_RECORD* exception_record, void* return_value,
                 CONTEXT* original_context,
                 UNWIND_HISTORY_TABLE* history_table);
RUNTIME_FUNCTION* RtlLookupFunctionEntry(uintptr_t pc, uintptr_t* base,
                                         UNWIND_HISTORY_TABLE* table);
EXCEPTION_ROUTINE RtlVirtualUnwind(uint32_t type, uint64_t image_base,
                                   uint64_t pc, RUNTIME_FUNCTION *func,
                                   CONTEXT* context, void** handler_data,
                                   uint64_t* establisher_frame,
                                   KNONVOLATILE_CONTEXT_POINTERS* ctx_ptr);
nt_status LdrFindEntryForAddress(void* address, LDR_DATA_TABLE_ENTRY** entry);
nt_status LdrGetDllHandle(const char16_t* dll_path, uint32_t* dll_characteristics,
                          const UNICODE_STRING* dll_name, void** dll_handle);

}

__inline void* GetProcessHeap() {
    return (void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap;
}

struct mn_peb_lock {
    void lock() {
        RtlAcquirePebLock();
    }

    void unlock() {
        RtlReleasePebLock();
    }
};

struct mn_loader_lock {
    void lock() {
        RtlAcquireLoaderLock();
    }

    void unlock() {
        RtlReleaseLoaderLock();
    }
};

};
