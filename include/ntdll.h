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

extern "C" {

void* RtlCreateHeap(uint32_t flags, void* heap_base, size_t reserve_size, size_t commit_size,
                    void* lock, RTL_HEAP_PARAMETERS* parameters);
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size);
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address);
void* RtlDestroyHeap(void* heap_handle);

}
