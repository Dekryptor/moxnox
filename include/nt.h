#pragma once

#include <stdint.h>

enum class nt_status : uint32_t {
    SUCCESS = 0x00000000,
    PENDING = 0x00000103,
    NOT_IMPLEMENTED = 0xc0000002,
    ACCESS_VIOLATION = 0xc0000005,
    INVALID_HANDLE = 0xc0000008,
    INVALID_PARAMETER = 0xc000000d,
    NO_MEMORY = 0xc0000017,
    OBJECT_TYPE_MISMATCH = 0xc0000024,
    INVALID_PAGE_PROTECTION = 0xc0000045,
    INSUFFICIENT_RESOURCES = 0xc000009a,
    INTERNAL_ERROR = 0xc00000e5
};

using NTSTATUS = enum nt_status;
using USHORT = uint16_t;
using PWSTR = char16_t*;
using HANDLE = uintptr_t;
using ULONG = uint32_t;
using LONG = int32_t;
using PVOID = void*;
using ULONG_PTR = uintptr_t;
using LONGLONG = int64_t;
using ACCESS_MASK = uint32_t;

static consteval HANDLE NtCurrentProcess() {
    return (HANDLE)-1;
}

static consteval HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

static constexpr bool success(NTSTATUS val) {
    return !((uint32_t)val & 0x80000000);
}

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};

struct IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
};

using PIO_APC_ROUTINE = void (*)(PVOID, const IO_STATUS_BLOCK*, ULONG);

union LARGE_INTEGER {
    struct {
        uint32_t LowPart;
        int32_t HighPart;
    };
    int64_t QuadPart;
};

union ULARGE_INTEGER {
    struct {
        uint32_t LowPart;
        uint32_t HighPart;
    };
    uint64_t QuadPart;
};

static const uint32_t MEM_COMMIT  = 0x00001000;
static const uint32_t MEM_RESERVE  = 0x00002000;
static const uint32_t MEM_DECOMMIT = 0x00004000;
static const uint32_t MEM_RELEASE = 0x00008000;
static const uint32_t MEM_RESET = 0x00080000;
static const uint32_t MEM_TOP_DOWN  = 0x00100000;
static const uint32_t MEM_PHYSICAL = 0x00400000;
static const uint32_t MEM_RESET_UNDO  = 0x01000000;
static const uint32_t MEM_LARGE_PAGES  = 0x20000000;

static const uint32_t PAGE_NOACCESS = 0x01;
static const uint32_t PAGE_READONLY = 0x02;
static const uint32_t PAGE_READWRITE = 0x04;
static const uint32_t PAGE_WRITECOPY = 0x08;
static const uint32_t PAGE_EXECUTE = 0x10;
static const uint32_t PAGE_EXECUTE_READ = 0x20;
static const uint32_t PAGE_EXECUTE_READWRITE = 0x40;
static const uint32_t PAGE_EXECUTE_WRITECOPY = 0x80;
static const uint32_t PAGE_GUARD = 0x100;
static const uint32_t PAGE_NOCACHE = 0x200;
static const uint32_t PAGE_WRITECOMBINE = 0x400;

struct EXCEPTION_REGISTRATION_RECORD;

struct NT_TIB {
    EXCEPTION_REGISTRATION_RECORD* ExceptionList;
    void* StackBase;
    void* StackLimit;
    void* SubSystemTib;
    union {
        void* FiberData;
        uint32_t Version;
    };
    void* ArbitraryUserPointer;
    NT_TIB* Self;
};

struct CLIENT_ID {
    void* UniqueProcess;
    void* UniqueThread;
};

struct GDI_TEB_BATCH {
    uint32_t Offset : 31;
    uint32_t HasRenderingCommand : 1;
    uint64_t HDC;
    uint32_t Buffer[310];
};

struct LIST_ENTRY {
    LIST_ENTRY* Flink;
    LIST_ENTRY* Blink;
};

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

struct PEB_LDR_DATA;
struct RTL_CRITICAL_SECTION;
struct ACTIVATION_CONTEXT_DATA;
struct ASSEMBLY_STORAGE_MAP;

struct CURDIR {
    UNICODE_STRING DosPath;
    void* Handle;
};

struct STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char* Buffer;
};

struct RTL_DRIVE_LETTER_CURDIR {
    uint16_t Flags;
    uint16_t Length;
    uint32_t TimeStamp;
    STRING DosPath;
};

struct RTL_USER_PROCESS_PARAMETERS {
    uint32_t MaximumLength;
    uint32_t Length;
    uint32_t Flags;
    uint32_t DebugFlags;
    void* ConsoleHandle;
    uint32_t ConsoleFlags;
    HANDLE StandardInput;
    HANDLE StandardOutput;
    HANDLE StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    void* Environment;
    uint32_t StartingX;
    uint32_t StartingY;
    uint32_t CountX;
    uint32_t CountY;
    uint32_t CountCharsX;
    uint32_t CountCharsY;
    uint32_t FillAttribute;
    uint32_t WindowFlags;
    uint32_t ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    RTL_DRIVE_LETTER_CURDIR CurrentDirectories[32];
    uint64_t EnvironmentSize;
    uint64_t EnvironmentVersion;
    void* PackageDependencyData;
    uint32_t ProcessGroupId;
    uint32_t LoaderThreads;
    UNICODE_STRING RedirectionDllName;
    UNICODE_STRING HeapPartitionName;
    uint64_t* DefaultThreadpoolCpuSetMasks;
    uint32_t DefaultThreadpoolCpuSetMaskCount;
    uint32_t DefaultThreadpoolThreadMaximum;
};

struct PEB {
    bool InheritedAddressSpace;
    bool ReadImageFileExecOptions;
    bool BeingDebugged;
    union {
        uint8_t BitField;
        struct {
            uint8_t ImageUsedLargePages : 1;
            uint8_t IsProtectedProcess : 1;
            uint8_t IsImageDynamicallyRelocated : 1;
            uint8_t SkipPatchingUser32Forwarders : 1;
            uint8_t IsPackagedProcess : 1;
            uint8_t IsAppContainer: 1;
            uint8_t IsProtectedProcessLight : 1;
            uint8_t IsLongPathAwareProcess : 1;
        };
    };
    uint8_t Padding0[4];
    HANDLE Mutant;
    void* ImageBaseAddress;
    PEB_LDR_DATA* Ldr;
    RTL_USER_PROCESS_PARAMETERS *ProcessParameters;
    void* SubSystemData;
    HANDLE ProcessHeap;
    RTL_CRITICAL_SECTION *FastPebLock;
    void* AtlThunkSListPtr;
    void* IFEOKey;
    union {
        uint32_t CrossProcessFlags;
        struct {
            uint32_t ProcessInJob : 1;
            uint32_t ProcessInitializing : 1;
            uint32_t ProcessUsingVEH : 1;
            uint32_t ProcessUsingVCH : 1;
            uint32_t ProcessUsingFTH : 1;
            uint32_t ProcessPreviouslyThrottled : 1;
            uint32_t ProcessCurrentlyThrottled : 1;
            uint32_t ProcessImagesHotPatched : 1;
            uint32_t ReservedBits0 : 24;
        };
    };
    uint8_t Padding1[4];
    union {
        void* KernelCallbackTable;
        void* UserSharedInfoPtr;
    };
    uint32_t SystemReserved;
    uint32_t AtlThunkSListPtr32;
    void* ApiSetMap;
    uint32_t TlsExpansionCounter;
    uint8_t Padding2[4];
    void* TlsBitmap;
    uint32_t TlsBitmapBits[2];
    void* ReadOnlySharedMemoryBase;
    void* SharedData;
    void** ReadOnlyStaticServerData;
    void* AnsiCodePageData;
    void* OemCodePageData;
    void* UnicodeCaseTableData;
    uint32_t NumberOfProcessors;
    uint32_t NtGlobalFlag;
    LARGE_INTEGER CriticalSectionTimeout;
    uintptr_t HeapSegmentReserve;
    uintptr_t HeapSegmentCommit;
    uintptr_t HeapDeCommitTotalFreeThreshold;
    uintptr_t HeapDeCommitFreeBlockThreshold;
    uint32_t NumberOfHeaps;
    uint32_t MaximumNumberOfHeaps;
    void**ProcessHeaps;
    void* GdiSharedHandleTable;
    void* ProcessStarterHelper;
    uint32_t GdiDCAttributeList;
    uint8_t Padding3[4];
    RTL_CRITICAL_SECTION* LoaderLock;
    uint32_t OSMajorVersion;
    uint32_t OSMinorVersion;
    uint16_t OSBuildNumber;
    uint16_t OSCSDVersion;
    uint32_t OSPlatformId;
    uint32_t ImageSubsystem;
    uint32_t ImageSubsystemMajorVersion;
    uint32_t ImageSubsystemMinorVersion;
    uint8_t Padding4[4];
    uintptr_t ActiveProcessAffinityMask;
    uint32_t GdiHandleBuffer[60];
    void* PostProcessInitRoutine;
    void* TlsExpansionBitmap;
    uint32_t TlsExpansionBitmapBits[32];
    uint32_t SessionId;
    uint8_t Padding5[4];
    ULARGE_INTEGER AppCompatFlags;
    ULARGE_INTEGER AppCompatFlagsUser;
    void* pShimData;
    void* AppCompatInfo;
    UNICODE_STRING CSDVersion;
    const ACTIVATION_CONTEXT_DATA* ActivationContextData;
    ASSEMBLY_STORAGE_MAP* ProcessAssemblyStorageMap;
    const ACTIVATION_CONTEXT_DATA* SystemDefaultActivationContextData;
    ASSEMBLY_STORAGE_MAP* SystemAssemblyStorageMap;
    uintptr_t MinimumStackCommit;
    void* SparePointers[4];
    uint32_t SpareUlongs[5];
    void* WerRegistrationData;
    void* WerShipAssertPtr;
    void* pUnused;
    void* pImageHeaderHash;
    union {
        uint32_t TracingFlags;
        struct {
            uint32_t HeapTracingEnabled : 1;
            uint32_t CritSecTracingEnabled : 1;
            uint32_t LibLoaderTracingEnabled : 1;
            uint32_t SpareTracingBits : 29;
        };
    };
    uint64_t CsrServerReadOnlySharedMemoryBase;
};

struct ACTIVATION_CONTEXT_STACK;
struct TEB_ACTIVE_FRAME;

struct TEB {
    NT_TIB NtTib;
    void* EnvironmentPointer;
    CLIENT_ID ClientId;
    void* ActiveRpcHandle;
    void* ThreadLocalStoragePointer;
    PEB* ProcessEnvironmentBlock;
    uint32_t LastErrorValue;
    uint32_t CountOfOwnedCriticalSections;
    void* CsrClientThread;
    void* Win32ThreadInfo;
    uint32_t User32Reserved[26];
    uint32_t UserReserved[5];
    void* WOW32Reserved;
    uint32_t CurrentLocale;
    uint32_t FpSoftwareStatusRegister;
    void* SystemReserved1[54];
    int32_t ExceptionCode;
    uint8_t Padding0[4];
    ACTIVATION_CONTEXT_STACK* ActivationContextStackPointer;
    uint8_t SpareBytes[24];
    uint32_t TxFsContext;
    uint8_t Padding1[4];
    GDI_TEB_BATCH GdiTebBatch;
    CLIENT_ID RealClientId;
    void* GdiCachedProcessHandle;
    uint32_t GdiClientPID;
    uint32_t GdiClientTID;
    void* GdiThreadLocalInfo;
    uintptr_t Win32ClientInfo[62];
    void* glDispatchTable[233];
    uintptr_t glReserved1[29];
    void* glReserved2;
    void* glSectionInfo;
    void* glSection;
    void* glTable;
    void* glCurrentRC;
    void* glContext;
    uint32_t LastStatusValue;
    uint8_t Padding2[4];
    UNICODE_STRING StaticUnicodeString;
    char16_t StaticUnicodeBuffer[261];
    uint8_t Padding3[6];
    void* DeallocationStack;
    void* TlsSlots[64];
    LIST_ENTRY TlsLinks;
    void* Vdm;
    void* ReservedForNtRpc;
    HANDLE DbgSsReserved[2];
    uint32_t HardErrorMode;
    uint8_t Padding4[4];
    void* Instrumentation[11];
    GUID ActivityId;
    void* SubProcessTag;
    void* PerflibData;
    void* EtwTraceData;
    void* WinSockData;
    uint32_t GdiBatchCount;
    struct {
        uint8_t ReservedPad0;
        uint8_t ReservedPad1;
        uint8_t ReservedPad2;
        uint8_t IdealProcessor;
    };
    uint32_t GuaranteedStackBytes;
    uint8_t Padding5[4];
    void* ReservedForPerf;
    void* ReservedForOle;
    uint32_t WaitingOnLoaderLock;
    uint8_t Padding6[4];
    void* SavedPriorityState;
    uintptr_t ReservedForCodeCoverage;
    void* ThreadPoolData;
    void** TlsExpansionSlots;
    void* DeallocationBStore;
    void* BStoreLimit;
    uint32_t MuiGeneration;
    uint32_t IsImpersonating;
    void* NlsCache;
    void* pShimData;
    uint16_t HeapVirtualAffinity;
    uint16_t LowFragHeapDataSlot;
    uint8_t Padding7[4];
    void* CurrentTransactionHandle;
    TEB_ACTIVE_FRAME* ActiveFrame;
    void* FlsData;
    void* PreferredLanguages;
    void* UserPrefLanguages;
    void* MergedPrefLanguages;
    uint32_t MuiImpersonation;
    uint16_t CrossTebFlags;
    union {
        uint16_t SameTebFlags;
        struct {
            uint16_t SafeThunkCall : 1;
            uint16_t InDebugPrint : 1;
            uint16_t HasFiberData : 1;
            uint16_t SkipThreadAttach : 1;
            uint16_t WerInShipAssertCode : 1;
            uint16_t RanProcessInit : 1;
            uint16_t ClonedThread : 1;
            uint16_t SuppressDebugMsg : 1;
            uint16_t DisableUserStackWalk : 1;
            uint16_t RtlExceptionAttached : 1;
            uint16_t InitialThread : 1;
            uint16_t SessionAware : 1;
            uint16_t LoadOwner : 1;
            uint16_t LoaderWorker : 1;
            uint16_t SpareSameTebBits : 2;
        };
    };
    void* TxnScopeEnterCallback;
    void* TxnScopeExitCallback;
    void* TxnScopeContext;
    uint32_t LockCount;
    uint32_t SpareUlong0;
    void* ResourceRetValue;
};

static __inline TEB* NtCurrentTeb() {
    TEB* ret;

    __asm("mov %0, gs:%1"
        : "=a" (ret)
        : "i" (offsetof(TEB, NtTib.Self))
    );

    return ret;
}

struct SECURITY_DESCRIPTOR;
struct SECURITY_QUALITY_OF_SERVICE;

struct OBJECT_ATTRIBUTES {
    uint32_t Length;
    HANDLE RootDirectory;
    UNICODE_STRING* ObjectName;
    uint32_t Attributes;
    SECURITY_DESCRIPTOR* SecurityDescriptor;
    SECURITY_QUALITY_OF_SERVICE* SecurityQualityOfService;
};

extern "C" {

NTSTATUS NtDisplayString(const UNICODE_STRING* string);
NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status);
NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status);
NTSTATUS NtWriteFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                     void* apc_context, IO_STATUS_BLOCK* io_status_block, const void* buffer,
                     uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t* key);
NTSTATUS NtResumeProcess(HANDLE proc_handle);
NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect);
NTSTATUS NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                             uint32_t free_type);
NTSTATUS NtReadFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                    void* apc_context, IO_STATUS_BLOCK* io_status_block, void* buffer,
                    uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t* key);
NTSTATUS NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                uint32_t new_prot, uint32_t* old_prot);
NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER* allocation_size, uint32_t file_attributes,
                      uint32_t share_access, uint32_t create_disposition, uint32_t create_options,
                      void* ea_buffer, uint32_t ea_length);
NTSTATUS NtClose(HANDLE handle);
NTSTATUS NtCreateSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                         const OBJECT_ATTRIBUTES* object_attributes, const LARGE_INTEGER* maximum_size,
                         uint32_t section_page_protection, uint32_t allocation_attributes,
                         HANDLE file_handle);

};
