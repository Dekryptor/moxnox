#pragma once

#include <stdint.h>
#include <stddef.h>

enum class nt_status : uint32_t {
    SUCCESS = 0x00000000,
    ALERTED = 0x00000101,
    TIMEOUT = 0x00000102,
    PENDING = 0x00000103,
    BREAKPOINT = 0x80000003,
    SINGLE_STEP = 0x80000004,
    NO_MORE_FILES = 0x80000006,
    NO_MORE_ENTRIES = 0x8000001a,
    NOT_IMPLEMENTED = 0xc0000002,
    INVALID_INFO_CLASS = 0xc0000003,
    INFO_LENGTH_MISMATCH = 0xc0000004,
    ACCESS_VIOLATION = 0xc0000005,
    INVALID_HANDLE = 0xc0000008,
    INVALID_PARAMETER = 0xc000000d,
    END_OF_FILE = 0xc0000011,
    NO_MEMORY = 0xc0000017,
    ILLEGAL_INSTRUCTION = 0xc000001d,
    ACCESS_DENIED = 0xc0000022,
    BUFFER_TOO_SMALL = 0xc0000023,
    OBJECT_TYPE_MISMATCH = 0xc0000024,
    INVALID_DISPOSITION = 0xc0000026,
    DISK_CORRUPT_ERROR =  0xc0000032,
    OBJECT_NAME_INVALID = 0xc0000033,
    OBJECT_NAME_NOT_FOUND = 0xc0000034,
    OBJECT_NAME_COLLISION = 0xc0000035,
    OBJECT_PATH_NOT_FOUND = 0xc000003a,
    SHARING_VIOLATION = 0xc0000043,
    INVALID_PAGE_PROTECTION = 0xc0000045,
    PROCEDURE_NOT_FOUND = 0xc000007a,
    INVALID_IMAGE_FORMAT = 0xc000007b,
    DISK_FULL = 0xc000007f,
    INTEGER_DIVIDE_BY_ZERO = 0xc0000094,
    INSUFFICIENT_RESOURCES = 0xc000009a,
    MEDIA_WRITE_PROTECTED = 0xc00000a2,
    PIPE_DISCONNECTED = 0xc00000b0,
    FILE_IS_A_DIRECTORY = 0xc00000ba,
    NOT_SUPPORTED = 0xc00000bb,
    INTERNAL_ERROR = 0xc00000e5,
    DIRECTORY_NOT_EMPTY = 0xc0000101,
    NOT_A_DIRECTORY = 0xc0000103,
    DLL_NOT_FOUND = 0xc0000135,
    ORDINAL_NOT_FOUND = 0xc0000138,
    CONTROL_C_EXIT = 0xc000013a,
    DLL_INIT_FAILED = 0xc0000142,
    UNHANDLED_EXCEPTION = 0xc0000144,
    NOT_FOUND = 0xc0000225,
    ILLEGAL_DLL_RELOCATION = 0xc0000269,
    SYSTEM_SHUTDOWN = 0xc00002eb
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
using KPRIORITY = int32_t;
using BOOLEAN = uint8_t;

static consteval HANDLE NtCurrentProcess() {
    return (HANDLE)-1;
}

static consteval HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

constexpr bool success(NTSTATUS val) {
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
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
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

struct RTL_CRITICAL_SECTION;
struct ACTIVATION_CONTEXT_DATA;
struct ASSEMBLY_STORAGE_MAP;

struct CURDIR {
    UNICODE_STRING DosPath;
    HANDLE Handle;
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
    HANDLE ConsoleHandle;
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

struct PEB_LDR_DATA {
    uint32_t Length;
    bool Initialized;
    void* SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    void* EntryInProgress;
    bool ShutdownInProgress;
    HANDLE ShutdownThreadId;
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
    RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
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

__inline __attribute((always_inline)) TEB* NtCurrentTeb() {
    TEB* ret;

    __asm("mov %0, gs:%1"
        : "=a" (ret)
        : "i" (offsetof(TEB, NtTib.Self))
    );

    return ret;
}

struct M128A {
    uint64_t Low;
    int64_t High;
} __attribute__ ((aligned (16)));

struct XSAVE_FORMAT {
    uint16_t ControlWord;
    uint16_t StatusWord;
    uint8_t TagWord;
    uint8_t Reserved1;
    uint16_t ErrorOpcode;
    uint32_t ErrorOffset;
    uint16_t ErrorSelector;
    uint16_t Reserved2;
    uint32_t DataOffset;
    uint16_t DataSelector;
    uint16_t Reserved3;
    uint32_t MxCsr;
    uint32_t MxCsr_Mask;
    M128A FloatRegisters[8];
    M128A XmmRegisters[16];
    uint8_t Reserved4[96];
} __attribute__ ((aligned (16)));

struct AMD64_CONTEXT {
    uint64_t P1Home;
    uint64_t P2Home;
    uint64_t P3Home;
    uint64_t P4Home;
    uint64_t P5Home;
    uint64_t P6Home;
    uint32_t ContextFlags;
    uint32_t MxCsr;
    uint16_t SegCs;
    uint16_t SegDs;
    uint16_t SegEs;
    uint16_t SegFs;
    uint16_t SegGs;
    uint16_t SegSs;
    uint32_t EFlags;
    uint64_t Dr0;
    uint64_t Dr1;
    uint64_t Dr2;
    uint64_t Dr3;
    uint64_t Dr6;
    uint64_t Dr7;
    uint64_t Rax;
    uint64_t Rcx;
    uint64_t Rdx;
    uint64_t Rbx;
    uint64_t Rsp;
    uint64_t Rbp;
    uint64_t Rsi;
    uint64_t Rdi;
    uint64_t R8;
    uint64_t R9;
    uint64_t R10;
    uint64_t R11;
    uint64_t R12;
    uint64_t R13;
    uint64_t R14;
    uint64_t R15;
    uint64_t Rip;
    union {
        XSAVE_FORMAT FltSave;
        struct {
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0;
            M128A Xmm1;
            M128A Xmm2;
            M128A Xmm3;
            M128A Xmm4;
            M128A Xmm5;
            M128A Xmm6;
            M128A Xmm7;
            M128A Xmm8;
            M128A Xmm9;
            M128A Xmm10;
            M128A Xmm11;
            M128A Xmm12;
            M128A Xmm13;
            M128A Xmm14;
            M128A Xmm15;
        };
    };
    M128A VectorRegister[26];
    uint64_t VectorControl;
    uint64_t DebugControl;
    uint64_t LastBranchToRip;
    uint64_t LastBranchFromRip;
    uint64_t LastExceptionToRip;
    uint64_t LastExceptionFromRip;
} __attribute__ ((aligned (16)));

using CONTEXT = AMD64_CONTEXT;

static const uint32_t CONTEXT_AMD64 = 0x00100000;
static const uint32_t CONTEXT_CONTROL = CONTEXT_AMD64 | 0x00000001;
static const uint32_t CONTEXT_INTEGER = CONTEXT_AMD64 | 0x00000002;
static const uint32_t CONTEXT_SEGMENTS = CONTEXT_AMD64 | 0x00000004;
static const uint32_t CONTEXT_FLOATING_POINT = CONTEXT_AMD64 | 0x00000008;
static const uint32_t CONTEXT_DEBUG_REGISTERS = CONTEXT_AMD64 | 0x00000010;

static const unsigned int EXCEPTION_MAXIMUM_PARAMETERS = 15;

struct EXCEPTION_RECORD {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    EXCEPTION_RECORD* ExceptionRecord;
    void* ExceptionAddress;
    uint32_t NumberParameters;
    uintptr_t ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};

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

enum class SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
};

static const uint32_t FILE_SUPERSEDE = 0x00000000;
static const uint32_t FILE_OPEN = 0x00000001;
static const uint32_t FILE_CREATE = 0x00000002;
static const uint32_t FILE_OPEN_IF = 0x00000003;
static const uint32_t FILE_OVERWRITE = 0x00000004;
static const uint32_t FILE_OVERWRITE_IF = 0x00000005;

static const uint32_t FILE_SUPERSEDED = 0x00000000;
static const uint32_t FILE_OPENED = 0x00000001;
static const uint32_t FILE_CREATED = 0x00000002;
static const uint32_t FILE_OVERWRITTEN = 0x00000003;
static const uint32_t FILE_EXISTS = 0x00000004;
static const uint32_t FILE_DOES_NOT_EXIST = 0x00000005;

static const uint32_t FILE_ATTRIBUTE_READONLY = 0x00000001;
static const uint32_t FILE_ATTRIBUTE_HIDDEN = 0x00000002;
static const uint32_t FILE_ATTRIBUTE_SYSTEM = 0x00000004;
static const uint32_t FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
static const uint32_t FILE_ATTRIBUTE_ARCHIVE = 0x00000020;
static const uint32_t FILE_ATTRIBUTE_DEVICE = 0x00000040;
static const uint32_t FILE_ATTRIBUTE_NORMAL = 0x00000080;
static const uint32_t FILE_ATTRIBUTE_TEMPORARY = 0x00000100;
static const uint32_t FILE_ATTRIBUTE_SPARSE_FILE = 0x00000200;
static const uint32_t FILE_ATTRIBUTE_REPARSE_POINT = 0x00000400;
static const uint32_t FILE_ATTRIBUTE_COMPRESSED = 0x00000800;
static const uint32_t FILE_ATTRIBUTE_OFFLINE = 0x00001000;
static const uint32_t FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x00002000;
static const uint32_t FILE_ATTRIBUTE_ENCRYPTED = 0x00004000;
static const uint32_t FILE_ATTRIBUTE_INTEGRITY_STREAM = 0x00008000;
static const uint32_t FILE_ATTRIBUTE_VIRTUAL = 0x00010000;
static const uint32_t FILE_ATTRIBUTE_NO_SCRUB_DATA = 0x00020000;
static const uint32_t FILE_ATTRIBUTE_EA = 0x00040000;
static const uint32_t FILE_ATTRIBUTE_PINNED = 0x00080000;
static const uint32_t FILE_ATTRIBUTE_UNPINNED = 0x00100000;
static const uint32_t FILE_ATTRIBUTE_RECALL_ON_OPEN = 0x00040000;
static const uint32_t FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS = 0x00400000;

static const uint32_t SEC_PARTITION_OWNER_HANDLE = 0x00040000;
static const uint32_t SEC_64K_PAGES = 0x00080000;
static const uint32_t SEC_FILE = 0x00800000;
static const uint32_t SEC_IMAGE = 0x01000000;
static const uint32_t SEC_PROTECTED_IMAGE = 0x02000000;
static const uint32_t SEC_RESERVE = 0x04000000;
static const uint32_t SEC_COMMIT = 0x08000000;
static const uint32_t SEC_NOCACHE = 0x10000000;
static const uint32_t SEC_WRITECOMBINE = 0x40000000;
static const uint32_t SEC_LARGE_PAGES = 0x80000000;

enum class FILE_INFORMATION_CLASS {
    DirectoryInformation = 1,
    FullDirectoryInformation,
    BothDirectoryInformation,
    BasicInformation,
    StandardInformation,
    InternalInformation,
    EaInformation,
    AccessInformation,
    NameInformation,
    RenameInformation,
    LinkInformation,
    NamesInformation,
    DispositionInformation,
    PositionInformation,
    FullEaInformation,
    ModeInformation,
    AlignmentInformation,
    AllInformation,
    AllocationInformation,
    EndOfFileInformation,
    AlternateNameInformation,
    StreamInformation,
    PipeInformation,
    PipeLocalInformation,
    PipeRemoteInformation,
    MailslotQueryInformation,
    MailslotSetInformation,
    CompressionInformation,
    ObjectIdInformation,
    CompletionInformation,
    MoveClusterInformation,
    QuotaInformation,
    ReparsePointInformation,
    NetworkOpenInformation,
    AttributeTagInformation,
    TrackingInformation,
    IdBothDirectoryInformation,
    IdFullDirectoryInformation,
    ValidDataLengthInformation,
    ShortNameInformation,
    IoCompletionNotificationInformation,
    IoStatusBlockRangeInformation,
    IoPriorityHintInformation,
    SfioReserveInformation,
    SfioVolumeInformation,
    HardLinkInformation,
    ProcessIdsUsingFileInformation,
    NormalizedNameInformation,
    NetworkPhysicalNameInformation,
    IdGlobalTxDirectoryInformation,
    IsRemoteDeviceInformation,
    UnusedInformation,
    NumaNodeInformation,
    StandardLinkInformation,
    RemoteProtocolInformation,
    RenameInformationBypassAccessCheck,
    LinkInformationBypassAccessCheck,
    VolumeNameInformation,
    IdInformation,
    IdExtdDirectoryInformation,
    ReplaceCompletionInformation,
    HardLinkFullIdInformation,
    IdExtdBothDirectoryInformation,
    DispositionInformationEx,
    RenameInformationEx,
    RenameInformationExBypassAccessCheck,
    DesiredStorageClassInformation,
    StatInformation,
    MemoryPartitionInformation,
    StatLxInformation,
    CaseSensitiveInformation,
    LinkInformationEx,
    LinkInformationExBypassAccessCheck,
    StorageReserveIdInformation,
    CaseSensitiveInformationForceAccessCheck,
    MaximumInformation
};

struct FILE_DIRECTORY_INFORMATION {
    uint32_t NextEntryOffset;
    uint32_t FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    uint32_t FileAttributes;
    uint32_t FileNameLength;
    char16_t FileName[1];
};

struct INITIAL_TEB {
    void* OldStackBase;
    void* OldStackLimit;
    void* StackBase;
    void* StackLimit;
    void* StackAllocationBase;
};

static const ACCESS_MASK DELETE = 0x00010000;
static const ACCESS_MASK READ_CONTROL = 0x00020000;
static const ACCESS_MASK WRITE_DAC = 0x00040000;
static const ACCESS_MASK WRITE_OWNER = 0x00080000;
static const ACCESS_MASK STANDARD_RIGHTS_REQUIRED = DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER;
static const ACCESS_MASK SYNCHRONIZE = 0x00100000;
static const ACCESS_MASK ACCESS_SYSTEM_SECURITY = 0x01000000;
static const ACCESS_MASK MAXIMUM_ALLOWED = 0x02000000;
static const ACCESS_MASK GENERIC_ALL = 0x10000000;
static const ACCESS_MASK GENERIC_EXECUTE = 0x20000000;
static const ACCESS_MASK GENERIC_WRITE = 0x40000000;
static const ACCESS_MASK GENERIC_READ = 0x80000000;

static const ACCESS_MASK THREAD_TERMINATE = 0x0001;
static const ACCESS_MASK THREAD_SUSPEND_RESUME = 0x0002;
static const ACCESS_MASK THREAD_ALERT = 0x0004;
static const ACCESS_MASK THREAD_GET_CONTEXT = 0x0008;
static const ACCESS_MASK THREAD_SET_CONTEXT = 0x0010;
static const ACCESS_MASK THREAD_SET_INFORMATION = 0x0020;
static const ACCESS_MASK THREAD_QUERY_INFORMATION = 0x0040;
static const ACCESS_MASK THREAD_SET_THREAD_TOKEN = 0x0080;
static const ACCESS_MASK THREAD_IMPERSONATE = 0x0100;
static const ACCESS_MASK THREAD_DIRECT_IMPERSONATION = 0x0200;
static const ACCESS_MASK THREAD_SET_LIMITED_INFORMATION = 0x0400;
static const ACCESS_MASK THREAD_QUERY_LIMITED_INFORMATION = 0x0800;
static const ACCESS_MASK THREAD_RESUME = 0x1000;
static const ACCESS_MASK THREAD_RESERVED1 = 0x2000;
static const ACCESS_MASK THREAD_RESERVED2 = 0x4000;
static const ACCESS_MASK THREAD_RESERVED3 = 0x8000;

static const ACCESS_MASK THREAD_ALL_ACCESS = THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_ALERT |
                                             THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION |
                                             THREAD_SET_INFORMATION | THREAD_SET_THREAD_TOKEN | THREAD_IMPERSONATE |
                                             THREAD_DIRECT_IMPERSONATION | THREAD_SET_LIMITED_INFORMATION |
                                             THREAD_QUERY_LIMITED_INFORMATION | THREAD_RESUME | THREAD_RESERVED1 |
                                             THREAD_RESERVED2 | THREAD_RESERVED3 | DELETE | READ_CONTROL |
                                             WRITE_DAC | WRITE_OWNER | SYNCHRONIZE;

static const uint32_t DUPLICATE_CLOSE_SOURCE = 0x00000001;
static const uint32_t DUPLICATE_SAME_ACCESS = 0x00000002;
static const uint32_t DUPLICATE_SAME_ATTRIBUTES = 0x00000004;

static const ACCESS_MASK FILE_READ_DATA = 0x00000001;
static const ACCESS_MASK FILE_LIST_DIRECTORY = 0x00000001;
static const ACCESS_MASK FILE_WRITE_DATA = 0x00000002;
static const ACCESS_MASK FILE_ADD_FILE = 0x00000002;
static const ACCESS_MASK FILE_APPEND_DATA = 0x00000004;
static const ACCESS_MASK FILE_ADD_SUBDIRECTORY = 0x00000004;
static const ACCESS_MASK FILE_CREATE_PIPE_INSTANCE = 0x00000004;
static const ACCESS_MASK FILE_READ_EA = 0x00000008;
static const ACCESS_MASK FILE_WRITE_EA = 0x00000010;
static const ACCESS_MASK FILE_EXECUTE = 0x00000020;
static const ACCESS_MASK FILE_TRAVERSE = 0x00000020;
static const ACCESS_MASK FILE_DELETE_CHILD = 0x00000040;
static const ACCESS_MASK FILE_READ_ATTRIBUTES = 0x00000080;
static const ACCESS_MASK FILE_WRITE_ATTRIBUTES = 0x00000100;
static const ACCESS_MASK FILE_ALL_ACCESS = WRITE_OWNER | WRITE_DAC | READ_CONTROL | DELETE |
                                           SYNCHRONIZE | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES |
                                           FILE_DELETE_CHILD | FILE_EXECUTE | FILE_WRITE_EA | FILE_READ_EA |
                                           FILE_APPEND_DATA | FILE_WRITE_DATA | FILE_READ_DATA;
static const ACCESS_MASK FILE_GENERIC_READ = READ_CONTROL | FILE_READ_DATA | FILE_READ_ATTRIBUTES |
                                             FILE_READ_EA | SYNCHRONIZE;
static const ACCESS_MASK FILE_GENERIC_WRITE = READ_CONTROL | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES |
                                              FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE;
static const ACCESS_MASK FILE_GENERIC_EXECUTE = READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_EXECUTE |
                                                SYNCHRONIZE;

static const uint32_t FILE_DIRECTORY_FILE = 0x00000001;
static const uint32_t FILE_WRITE_THROUGH = 0x00000002;
static const uint32_t FILE_SEQUENTIAL_ONLY = 0x00000004;
static const uint32_t FILE_NO_INTERMEDIATE_BUFFERING = 0x00000008;
static const uint32_t FILE_SYNCHRONOUS_IO_ALERT = 0x00000010;
static const uint32_t FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020;
static const uint32_t FILE_NON_DIRECTORY_FILE = 0x00000040;
static const uint32_t FILE_CREATE_TREE_CONNECTION = 0x00000080;
static const uint32_t FILE_COMPLETE_IF_OPLOCKED = 0x00000100;
static const uint32_t FILE_NO_EA_KNOWLEDGE = 0x00000200;
static const uint32_t FILE_OPEN_REMOTE_INSTANCE = 0x00000400;
static const uint32_t FILE_RANDOM_ACCESS = 0x00000800;
static const uint32_t FILE_DELETE_ON_CLOSE = 0x00001000;
static const uint32_t FILE_OPEN_BY_FILE_ID = 0x00002000;
static const uint32_t FILE_OPEN_FOR_BACKUP_INTENT = 0x00004000;
static const uint32_t FILE_NO_COMPRESSION = 0x00008000;
static const uint32_t FILE_OPEN_REQUIRING_OPLOCK = 0x00010000;
static const uint32_t FILE_DISALLOW_EXCLUSIVE = 0x00020000;
static const uint32_t FILE_RESERVE_OPFILTER = 0x00100000;
static const uint32_t FILE_OPEN_REPARSE_POINT = 0x00200000;
static const uint32_t FILE_OPEN_NO_RECALL = 0x00400000;
static const uint32_t FILE_OPEN_FOR_FREE_SPACE_QUERY = 0x00800000;

static const ACCESS_MASK SECTION_QUERY = 0x00000001;
static const ACCESS_MASK SECTION_MAP_WRITE = 0x00000002;
static const ACCESS_MASK SECTION_MAP_READ = 0x00000004;
static const ACCESS_MASK SECTION_MAP_EXECUTE = 0x00000008;
static const ACCESS_MASK SECTION_EXTEND_SIZE = 0x00000010;
static const ACCESS_MASK SECTION_MAP_EXECUTE_EXPLICIT = 0x00000020;
static const ACCESS_MASK SECTION_ALL_ACCESS = DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER | SECTION_QUERY |
                                              SECTION_MAP_WRITE | SECTION_MAP_READ | SECTION_MAP_EXECUTE |
                                              SECTION_EXTEND_SIZE;

enum class PROCESSINFOCLASS {
    BasicInformation,
    QuotaLimits,
    IoCounters,
    VmCounters,
    Times,
    BasePriority,
    RaisePriority,
    DebugPort,
    ExceptionPort,
    AccessToken,
    LdtInformation,
    LdtSize,
    DefaultHardErrorMode,
    IoPortHandlers,
    PooledUsageAndLimits,
    WorkingSetWatch,
    UserModeIOPL,
    EnableAlignmentFaultFixup,
    PriorityClass,
    Wx86Information,
    HandleCount,
    AffinityMask,
    PriorityBoost,
    DeviceMap,
    SessionInformation,
    ForegroundInformation,
    Wow64Information,
    ImageFileName,
    LUIDDeviceMapsEnabled,
    BreakOnTermination,
    DebugObjectHandle,
    DebugFlags,
    HandleTracing,
    IoPriority,
    ExecuteFlags,
    TlsInformation,
    Cookie,
    ImageInformation,
    CycleTime,
    PagePriority,
    InstrumentationCallback,
    ThreadStackAllocation,
    WorkingSetWatchEx,
    ImageFileNameWin32,
    ImageFileMapping,
    AffinityUpdateMode,
    MemoryAllocationMode,
    GroupInformation,
    TokenVirtualizationEnabled,
    OwnerInformation,
    WindowInformation,
    HandleInformation,
    MitigationPolicy,
    DynamicFunctionTableInformation,
    HandleCheckingMode,
    KeepAliveCount,
    RevokeFileHandles,
    WorkingSetControl,
    HandleTable,
    CheckStackExtentsMode,
    CommandLineInformation,
    ProtectionInformation,
    MemoryExhaustion,
    FaultInformation,
    TelemetryIdInformation,
    CommitReleaseInformation,
    Reserved1Information,
    Reserved2Information,
    SubsystemProcess,
    InPrivate = 70,
    RaiseUMExceptionOnInvalidHandleClose,
    SubsystemInformation = 75,
    Win32kSyscallFilterInformation = 79,
    EnergyTrackingState = 82
};

struct PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PEB* PebBaseAddress;
    uintptr_t AffinityMask;
    KPRIORITY BasePriority;
    uintptr_t UniqueProcessId;
    uintptr_t InheritedFromUniqueProcessId;
};

enum class SECTION_INFORMATION_CLASS {
    BasicInformation,
    ImageInformation,
    RelocationInformation,
    OriginalBaseInformation,
    InternalImageInformation,

    MoxnoxFramebufferInformation = 0x1000
};

struct SECTION_IMAGE_INFORMATION {
    void* TransferAddress;
    uint32_t ZeroBits;
    size_t MaximumStackSize;
    size_t CommittedStackSize;
    uint32_t SubSystemType;
    union {
        struct {
            uint16_t SubSystemMinorVersion;
            uint16_t SubSystemMajorVersion;
        };
        uint32_t SubSystemVersion;
    };
    uint32_t GpValue;
    uint16_t ImageCharacteristics;
    uint16_t DllCharacteristics;
    uint16_t Machine;
    bool ImageContainsCode;
    union {
        uint8_t ImageFlags;
        struct {
            uint8_t ComPlusNativeReady : 1;
            uint8_t ComPlusILOnly : 1;
            uint8_t ImageDynamicallyRelocated : 1;
            uint8_t ImageMappedFlat : 1;
            uint8_t BaseBelow4gb : 1;
            uint8_t Reserved : 3;
        };
    };
    uint32_t LoaderFlags;
    uint32_t ImageFileSize;
    uint32_t CheckSum;
};

struct MN_SECTION_FRAMEBUFFER_INFORMATION {
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
    uint32_t dpi;
};

enum class THREAD_INFORMATION_CLASS {
    BasicInformation,
    Times,
    Priority,
    BasePriority,
    AffinityMask,
    ImpersonationToken,
    DescriptorTableEntry,
    EnableAlignmentFaultFixup,
    EventPair_Reusable,
    QuerySetWin32StartAddress,
    ZeroTlsCell,
    PerformanceCount,
    AmILastThread,
    IdealProcessor,
    PriorityBoost,
    SetTlsArrayAddress,
    IsIoPending,
    HideFromDebugger,
    BreakOnTermination,
    SwitchLegacyState,
    IsTerminated,
    LastSystemCall,
    IoPriority,
    CycleTime,
    PagePriority,
    ActualBasePriority,
    TebInformation,
    CSwitchMon,
    CSwitchPmu,
    Wow64Context,
    GroupInformation,
    UmsInformation,
    CounterProfiling,
    IdealProcessorEx,
    CpuAccountingInformation,
    SuspendCount,
    HeterogeneousCpuPolicy,
    ContainerId,
    NameInformation,
    SelectedCpuSets,
    SystemThreadInformation,
    ActualGroupAffinity,
    DynamicCodePolicyInfo,
    ExplicitCaseSensitivity,
    WorkOnBehalfTicket,
    SubsystemInformation,
    DbgkWerReportActive,
    AttachContainer,
    ManageWritesToExecutableMemory,
    PowerThrottlingState,
    WorkloadClass
};

static const ACCESS_MASK TIMER_QUERY_STATE = 0x0001;
static const ACCESS_MASK TIMER_MODIFY_STATE = 0x0002;

static const ACCESS_MASK TIMER_ALL_ACCESS = TIMER_QUERY_STATE | TIMER_MODIFY_STATE | DELETE |
                                            READ_CONTROL | WRITE_DAC | WRITE_OWNER | SYNCHRONIZE;

enum class TIMER_TYPE {
    NotificationTimer,
    SynchronizationTimer
};

using PTIMER_APC_ROUTINE = void (*)(void*, uint32_t, int32_t);

struct RUNTIME_FUNCTION {
    uint32_t BeginAddress;
    uint32_t EndAddress;
    union {
        uint32_t UnwindInfoAddress;
        uint32_t UnwindData;
    };
};

static const uint8_t UNW_FLAG_EHANDLER = 0x01;
static const uint8_t UNW_FLAG_UHANDLER = 0x02;
static const uint8_t UNW_FLAG_CHAININFO = 0x04;

struct UNWIND_INFO {
    uint8_t version : 3;
    uint8_t flags : 5;
    uint8_t prologue_bytes;
    uint8_t unwind_codes_count;
    uint8_t frame_register : 4;
    uint8_t frame_offset : 4;
};

struct UNWIND_CODE {
    uint8_t offset;
    uint8_t code : 4;
    uint8_t info : 4;
};

enum class SHUTDOWN_ACTION {
    ShutdownNoReboot,
    ShutdownReboot,
    ShutdownPowerOff
};

struct FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    uint32_t FileAttributes;
};

struct FILE_STANDARD_INFORMATION {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    uint32_t NumberOfLinks;
    uint8_t DeletePending;
    uint8_t Directory;
};

struct FILE_DISPOSITION_INFORMATION {
    BOOLEAN DeleteFile;
};

static const uint32_t MAXIMUM_WAIT_OBJECTS = 64;

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
NTSTATUS NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle, void** base_address,
                            uintptr_t zero_bits, size_t commit_size, LARGE_INTEGER* section_offset,
                            size_t* view_size, enum SECTION_INHERIT inherit_disposition,
                            uint32_t allocation_type, uint32_t win32_protect);
NTSTATUS NtUnmapViewOfSection(HANDLE process_handle, void* base_address);
NTSTATUS NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                              void* apc_context, IO_STATUS_BLOCK* io_status_block,
                              void* file_information, uint32_t length,
                              FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                              const UNICODE_STRING* file_name, bool restart_scan);
NTSTATUS NtCreateThread(HANDLE* thread_handle, ACCESS_MASK desired_access,
                        const OBJECT_ATTRIBUTES* object_attributes, HANDLE process_handle,
                        CLIENT_ID* client_id, const CONTEXT* thread_context,
                        const INITIAL_TEB* initial_teb, bool create_suspended);
NTSTATUS NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count);
NTSTATUS NtWaitForSingleObject(HANDLE object_handle, bool alertable, const LARGE_INTEGER* time_out);
NTSTATUS NtWaitForMultipleObjects(uint32_t object_count, HANDLE* objects_array,
                                  bool wait_any, bool alertable,
                                  const LARGE_INTEGER* time_out);
NTSTATUS NtCreateProcess(HANDLE* process_handle, ACCESS_MASK desired_access, const OBJECT_ATTRIBUTES* object_attributes,
                         HANDLE parent_process, bool inherit_object_table, HANDLE section_handle,
                         HANDLE debug_port, HANDLE exception_port);
NTSTATUS NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
                           HANDLE* target_handle, ACCESS_MASK desired_access, uint32_t handle_attributes,
                           uint32_t options);
NTSTATUS NtReadVirtualMemory(HANDLE process_handle, void* base_address, void* buffer, uint32_t buffer_size,
                             uint32_t* number_of_bytes_read);
NTSTATUS NtWriteVirtualMemory(HANDLE process_handle, void* base_address, const void* buffer, uint32_t buffer_size,
                              uint32_t* number_of_bytes_written);
NTSTATUS NtQueryInformationProcess(HANDLE process_handle, enum PROCESSINFOCLASS process_information_class,
                                   void* process_information, uint32_t process_information_length,
                                   uint32_t* return_length);
NTSTATUS NtQuerySection(HANDLE section_handle, enum SECTION_INFORMATION_CLASS section_information_class,
                        void* section_information, size_t section_information_length, size_t* return_length);
NTSTATUS NtContinue(const CONTEXT* thread_context, bool raise_alert);
NTSTATUS NtQueryInformationThread(HANDLE thread_handle, enum THREAD_INFORMATION_CLASS thread_information_class,
                                  void* thread_information, uint32_t thread_information_length,
                                  uint32_t* return_length);
NTSTATUS NtQuerySystemTime(LARGE_INTEGER* SystemTime);
NTSTATUS NtCreateTimer(HANDLE* timer_handle, ACCESS_MASK desired_access, const OBJECT_ATTRIBUTES* object_attributes,
                       TIMER_TYPE timer_type);
NTSTATUS NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time, PTIMER_APC_ROUTINE timer_apc_routine,
                    void* timer_context, bool resume_timer, int32_t period, bool* previous_state);
NTSTATUS NtCancelTimer(HANDLE timer_handle, bool* current_state);
NTSTATUS NtShutdownSystem(enum SHUTDOWN_ACTION action);
NTSTATUS NtDeviceIoControlFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine, void* apc_context,
                               IO_STATUS_BLOCK* iosb, uint32_t control_code, const void* input_buffer,
                               uint32_t input_buffer_len, void* output_buffer, uint32_t output_buffer_len);
NTSTATUS NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer, uint32_t buffer_size,
                              FILE_INFORMATION_CLASS file_information_class);
NTSTATUS NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer, uint32_t buffer_size,
                                FILE_INFORMATION_CLASS file_information_class);
NTSTATUS NtFlushVirtualMemory(HANDLE process_handle, void** base_address, size_t* region_size,
                              IO_STATUS_BLOCK* iosb);
NTSTATUS NtOpenSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* object_attributes);
NTSTATUS NtCreateNamedPipeFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                               const OBJECT_ATTRIBUTES* object_attributes,
                               IO_STATUS_BLOCK* iosb, uint32_t share_access,
                               uint32_t create_disposition, uint32_t create_options,
                               uint32_t named_pipe_type, uint32_t read_mode,
                               uint32_t completion_mode, uint32_t maximum_instances,
                               uint32_t inbound_quota, uint32_t outbound_quota,
                               const LARGE_INTEGER* default_timeout);
NTSTATUS NtOpenFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                    const OBJECT_ATTRIBUTES* object_attributes,
                    IO_STATUS_BLOCK* iosb, uint32_t share_access,
                    uint32_t open_options);
NTSTATUS NtGetNextThread(HANDLE process_handle, HANDLE thread_handle,
                         ACCESS_MASK desired_access, uint32_t handle_attributes,
                         uint32_t flags, HANDLE* new_thread_handle);
NTSTATUS NtSuspendThread(HANDLE thread_handle, uint32_t* previous_suspend_count);
NTSTATUS NtGetContextThread(HANDLE thread_handle, CONTEXT* context);
NTSTATUS NtSetContextThread(HANDLE thread_handle, const CONTEXT* context);
};
