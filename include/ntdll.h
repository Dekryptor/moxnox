#pragma once

#include <stdint.h>
#include <nt.h>

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

static const uint32_t HEAP_NO_SERIALIZE = 0x00000001;
static const uint32_t HEAP_GROWABLE  = 0x00000002;
static const uint32_t HEAP_GENERATE_EXCEPTIONS = 0x00000004;
static const uint32_t HEAP_ZERO_MEMORY = 0x00000008;
static const uint32_t HEAP_REALLOC_IN_PLACE_ONLY = 0x00000010;
static const uint32_t HEAP_TAIL_CHECKING_ENABLED = 0x00000020;
static const uint32_t HEAP_FREE_CHECKING_ENABLED = 0x00000040;
static const uint32_t HEAP_DISABLE_COALESCE_ON_FREE = 0x00000080;
static const uint32_t HEAP_CREATE_ALIGN_16 = 0x00010000;
static const uint32_t HEAP_CREATE_ENABLE_TRACING = 0x00020000;
static const uint32_t HEAP_CREATE_ENABLE_EXECUTE = 0x00040000;

struct LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InProgressLinks;
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

static const uint32_t DLL_PROCESS_DETACH = 0;
static const uint32_t DLL_PROCESS_ATTACH = 1;
static const uint32_t DLL_THREAD_ATTACH = 2;
static const uint32_t DLL_THREAD_DETACH = 3;

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
};

enum class EXCEPTION_DISPOSITION : int {
    ExceptionContinueExecution = 0,
    ExceptionContinueSearch = 1,
    ExceptionNestedException = 2,
    ExceptionCollidedUnwind = 3,
    ExceptionExecuteHandler = 4
};

using EXCEPTION_ROUTINE = EXCEPTION_DISPOSITION(*)(EXCEPTION_RECORD*, void*,
                                                   CONTEXT*, void*);

struct DISPATCHER_CONTEXT_AMD64 {
    uint64_t ControlPc;
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
    uint64_t EstablisherFrame;
    uint64_t TargetIp;
    AMD64_CONTEXT* ContextRecord;
    EXCEPTION_ROUTINE* LanguageHandler;
    void* HandlerData;
};

using DISPATCHER_CONTEXT = DISPATCHER_CONTEXT_AMD64;

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

}

static __inline void* GetProcessHeap() {
    return (void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap;
}
