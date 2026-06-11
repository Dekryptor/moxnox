#include <stdint.h>
#include <new>
#include <algorithm>

import nt;
import print;
import list;
import ntdll;
import ntdll_private;
import crt;
import pagesize;

using namespace std;

struct alloc_header {
    uint32_t size : 31;
    uint32_t free : 1;
    uint8_t _pad[12];
};

static_assert(sizeof(alloc_header) == 16);

struct free_block : alloc_header {
    list_entry le;
};

class subheap {
public:
    subheap(size_t reserve_size, size_t commit_size, list_entry& free_list);

    list_entry le;
    size_t reserve_size;
    size_t commit_size;
    uint8_t data[0];
};

class heap {
public:
    heap(size_t reserve_size, size_t commit_size);
    ~heap();

    void* allocate(uint32_t flags, size_t size);
    void* reallocate(uint32_t flags, void* ptr, size_t size);
    bool free(uint32_t flags, void* base_address);

    list_entry subheaps = { &subheaps, &subheaps };
    RTL_CRITICAL_SECTION cs;
    list_entry free_blocks = { &free_blocks, &free_blocks };
};

subheap::subheap(size_t reserve_size, size_t commit_size, list_entry& free_list) : reserve_size(reserve_size), commit_size(commit_size) {
    auto& fb = *(free_block*)data;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    fb.size = (uint32_t)(commit_size - offsetof(subheap, data) - sizeof(alloc_header));
    fb.size -= sizeof(alloc_header); // for sentinel
#pragma GCC diagnostic pop
    fb.free = 1;
    insert_tail_list(free_list, fb.le);

    auto& sentinel = *(alloc_header*)((uint8_t*)&fb + sizeof(alloc_header) + fb.size);
    sentinel.size = 0;
    sentinel.free = 0;
}

heap::heap(size_t reserve_size, size_t commit_size) {
    void* addr = nullptr;

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &reserve_size,
                                       MEM_RESERVE, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        // FIXME - return error
        return;
    }

    ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &commit_size,
                                  MEM_COMMIT, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        // FIXME - return error
        return;
    }

    auto sh = new (addr) subheap(reserve_size, commit_size, free_blocks);

    insert_tail_list(subheaps, sh->le);

    RtlInitializeCriticalSection(&cs);
}

heap::~heap() {
    RtlDeleteCriticalSection(&cs);
    auto le = subheaps.next;

    while (le != &subheaps) {
        auto& sh = containing_record<subheap>(le);
        auto next = le->next;

        void* addr = &sh;
        size_t region_size = 0;

        NtFreeVirtualMemory(NtCurrentProcess(), &addr, &region_size, MEM_RELEASE);

        le = next;
    }
}

static void mark_block_allocated(free_block& fb, size_t size,
                                 list_entry& free_blocks) {
    if (fb.size >= size + sizeof(free_block)) {
        auto& nh = *(free_block*)((uint8_t*)&fb + sizeof(alloc_header) + size);

        nh.free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        nh.size = fb.size - size - sizeof(alloc_header);
#pragma GCC diagnostic pop

        insert_tail_list(free_blocks, nh.le);
    }

    fb.free = 0;

    remove_entry_list(fb.le);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    fb.size = size;
#pragma GCC diagnostic pop
}

void* heap::allocate(uint32_t flags, size_t size) {
    // FIXME - flags

    // round to multiple of 16
    if (size & 15)
        size = ((size >> 4) + 1) << 4;

    if (size == 0 || size >= 0x80000000)
        return nullptr;

    RtlEnterCriticalSection(&cs);

    for (auto le = free_blocks.next; le != &free_blocks; le = le->next) {
        auto& fb = containing_record<free_block>(le);

        if (fb.size < size)
            continue;

        mark_block_allocated(fb, size, free_blocks);

        auto ret = (uint8_t*)&fb + sizeof(alloc_header);

        if (flags & HEAP_ZERO_MEMORY)
            memset(ret, 0, size);

        RtlLeaveCriticalSection(&cs);

        return ret;
    }

    // FIXME - return nullptr if heap not growable
    // FIXME - separate logic if "large" allocation?

    void* sh_addr = nullptr;
    size_t subheap_size = offsetof(subheap, data) + sizeof(alloc_header) +
                          size + sizeof(alloc_header);

    auto status = NtAllocateVirtualMemory(NtCurrentProcess(), &sh_addr, 0, &subheap_size,
                                          MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        RtlLeaveCriticalSection(&cs);
        return nullptr;
    }

    auto sh = new (sh_addr) subheap(subheap_size, subheap_size, free_blocks);

    insert_tail_list(subheaps, sh->le);

    auto& shfb = *(free_block*)sh->data;

    mark_block_allocated(shfb, size, free_blocks);

    auto ret = (uint8_t*)&shfb + sizeof(alloc_header);

    if (flags & HEAP_ZERO_MEMORY)
        memset(ret, 0, size);

    RtlLeaveCriticalSection(&cs);
    return ret;
}

void* heap::reallocate(uint32_t flags, void* ptr, size_t size) {
    // FIXME - flags?

    if (!ptr)
        return allocate(flags, size);

    if (size == 0) {
        free(flags, ptr);
        return nullptr;
    }

    // round to multiple of 16, same as allocate
    size_t rounded_size = size;
    if (rounded_size & 15)
        rounded_size = ((rounded_size >> 4) + 1) << 4;

    RtlEnterCriticalSection(&cs);

    auto ah = (alloc_header*)((uint8_t*)ptr - sizeof(alloc_header));
    auto old_size = (size_t)ah->size;

    RtlLeaveCriticalSection(&cs);

    if (rounded_size <= old_size)
        return ptr;

    auto ret = allocate(flags, size);
    if (!ret)
        return nullptr;

    memcpy(ret, ptr, old_size);

    free(flags, ptr);

    return ret;
}

bool heap::free(uint32_t, void* base_address) {
    // FIXME - flags

    RtlEnterCriticalSection(&cs);

    auto& fb = *(free_block*)((uint8_t*)base_address - sizeof(alloc_header));

    fb.free = 1;
    insert_tail_list(free_blocks, fb.le);

    // FIXME - decommit part of heap if last page(s) now clear?

    RtlLeaveCriticalSection(&cs);

    return true;
}

static constexpr size_t round_up(size_t val, uint64_t div) {
    if (val & (div - 1)) {
        val &= ~(div - 1);
        val += div;
    }

    return val;
}

extern "C"
void* RtlCreateHeap(uint32_t, void*, size_t reserve_size, size_t commit_size,
                    void*, RTL_HEAP_PARAMETERS*) {
    void* addr = nullptr;

    // FIXME - flags
    // FIXME - heap_base
    // FIXME - parameters

    reserve_size = round_up(reserve_size, PAGE_SIZE);
    commit_size = round_up(commit_size, PAGE_SIZE);

    // see https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlcreateheap
    if (reserve_size == 0) {
        if (commit_size == 0) {
            reserve_size = 64 << PAGE_SHIFT;
            commit_size = PAGE_SIZE;
        } else
            reserve_size = round_up(commit_size, 16 * PAGE_SIZE);
    } else {
        if (commit_size == 0)
            commit_size = PAGE_SIZE;
        else if (commit_size > reserve_size)
            commit_size = reserve_size;
    }

    size_t heap_obj_size = sizeof(heap);

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &heap_obj_size,
                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        return nullptr;
    }

    auto h = new (addr) heap(reserve_size, commit_size);

    return h;
}

extern "C"
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.allocate(flags, size);
}

extern "C"
void* RtlReAllocateHeap(void* heap_handle, uint32_t flags, void* ptr,
                        size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.reallocate(flags, ptr, size);
}

extern "C"
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address) {
    if (!base_address)
        return true;

    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.free(flags, base_address);
}

extern "C"
void* RtlDestroyHeap(void* heap_handle) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    h.heap::~heap();

    size_t region_size = 0;

    auto ret = NtFreeVirtualMemory(NtCurrentProcess(), &heap_handle, &region_size, MEM_RELEASE);

    if (!success(ret)) {
        print("NtFreeVirtualMemory returned {}\n", ret);
        return nullptr;
    }

    return nullptr;
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, ptr);
}
