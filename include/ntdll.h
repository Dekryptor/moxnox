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

extern "C" {

void* RtlCreateHeap(uint32_t flags, void* heap_base, size_t reserve_size, size_t commit_size,
                    void* lock, RTL_HEAP_PARAMETERS* parameters);
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size);
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address);
void* RtlDestroyHeap(void* heap_handle);

}
