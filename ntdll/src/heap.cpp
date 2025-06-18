#include "ntdll_private.h"
#include <string.h>

import print;
import list;

class subheap {
public:
    subheap(size_t reserve_size, size_t commit_size);

    void* allocate(uint32_t flags, size_t size);

    list_entry le;
    size_t reserve_size;
    size_t commit_size;
    uint8_t data[0];
};

class heap {
public:
    heap(size_t reserve_size, size_t commit_size);

    void* allocate(uint32_t flags, size_t size);
    bool free(uint32_t flags, void* base_address);

    list_entry subheaps = { &subheaps, &subheaps };
};

struct alloc_header {
    uint32_t size : 31;
    uint32_t free : 1;
};

static_assert(sizeof(alloc_header) == 4);

subheap::subheap(size_t reserve_size, size_t commit_size) : reserve_size(reserve_size), commit_size(commit_size) {
    auto& ah = *(alloc_header*)data;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    ah.size = (uint32_t)(commit_size - offsetof(subheap, data) - sizeof(alloc_header));
#pragma GCC diagnostic pop
    ah.free = 1;
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

    auto sh = new (addr) subheap(reserve_size, commit_size);

    insert_tail_list(subheaps, sh->le);
}

void* subheap::allocate(uint32_t flags, size_t size) {
    alloc_header* ah = nullptr;

    // allocate from existing pages if possible

    auto h = (alloc_header*)data;
    alloc_header* last = nullptr;

    do {
        if (h->free) {
            if (last && last->free) { // merge adjacent free entries
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                last->size += h->size + sizeof(alloc_header);
#pragma GCC diagnostic pop
                h = last;
            }

            if (h->size == size) {
                h->free = 0;

                auto ret = (uint8_t*)h + sizeof(alloc_header);

                if (flags & HEAP_ZERO_MEMORY)
                    memset(ret, 0, size);

                return ret;
            }

            if (h->size > size) {
                auto nh = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + size);

                if (nh < (void*)((uint8_t*)this + commit_size)) {
                    nh->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    nh->size = h->size - size - sizeof(alloc_header);
#pragma GCC diagnostic pop
                }

                h->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                h->size = size;
#pragma GCC diagnostic pop

                auto ret = (uint8_t*)h + sizeof(alloc_header);

                if (flags & HEAP_ZERO_MEMORY)
                    memset(ret, 0, size);

                return ret;
            }
        }

        last = h;
        h = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + h->size);
    } while (h < (void*)((uint8_t*)this + commit_size));

    if (last && last->free)
        ah = last;

    // allocate new pages

    unsigned int new_pages = bytes_to_pages(sizeof(alloc_header) + size);

    if (new_pages << PAGE_SHIFT > reserve_size - commit_size)
        return nullptr;

    void* addr = (uint8_t*)this + commit_size;
    size_t new_commit_size = new_pages << PAGE_SHIFT;

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &new_commit_size,
                                       MEM_COMMIT, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        return nullptr;
    }

    if (ah) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        ah->size += new_commit_size;
#pragma GCC diagnostic pop
        // FIXME - handle case when size would overflow 31 bits
    } else {
        ah = (alloc_header*)((uint8_t*)this + commit_size);
        ah->free = 1;
        ah->size = PAGE_SIZE - sizeof(alloc_header);
    }

    commit_size += new_commit_size;

    auto ptr = (uint8_t*)ah + sizeof(alloc_header);
    ah->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    ah->size = size;
#pragma GCC diagnostic pop

    auto ah2 = (alloc_header*)((uint8_t*)ptr + size);

    if ((uintptr_t)ah2 & (PAGE_SIZE - 1)) {
        ah2->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        ah2->size = (uint32_t)(((uintptr_t)this + commit_size) - (uintptr_t)ah2 - sizeof(alloc_header));
#pragma GCC diagnostic pop
    }

    // NtAllocateVirtualMemory guarantees initialized to 0

    return ptr;
}

void* heap::allocate(uint32_t flags, size_t size) {
    // FIXME - flags

    // round to multiple of 4
    if (size & 3)
        size = ((size >> 2) + 1) << 2;

    if (size == 0 || size >= 0x80000000)
        return nullptr;

    // FIXME - lock (critical section?)

    // FIXME - if subheap full, allocate new one
    // FIXME - separate subheap for allocations larger than 1 page?

    for (auto le = subheaps.next; le != &subheaps; le = le->next) {
        auto& sh = containing_record<subheap>(le);

        auto ret = sh.allocate(flags, size);
        if (ret)
            return ret;
    }

    print("FIXME - allocate new subheap (current subheap exhausted)\n");

    return nullptr;
}

bool heap::free(uint32_t, void* base_address) {
    // FIXME - flags

    // FIXME - lock

    auto ah = (alloc_header*)((uint8_t*)base_address - sizeof(alloc_header));

    ah->free = 1;

    // FIXME - decommit part of heap if last page(s) now clear?

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
    // FIXME - lock
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
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address) {
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
